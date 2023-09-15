/* USRP TX code, following the same structure of limesdr */
#define _CRT_SECURE_NO_WARNINGS

#include "usrpgps.h"
#include <math.h>
#include <unistd.h>
#include <signal.h>

#define EXECUTE_OR_GOTO(label, ...) \
    if (__VA_ARGS__)                \
    {                               \
        return_code = EXIT_FAILURE; \
        goto label;                 \
    }

bool stop_signal_called = false;

void sigint_handler(int code)
{
    (void)code;
    stop_signal_called = true;
    fprintf(stderr, "Done\n");
}

void init_sim(sim_t *s)
{
    printf("\nGPS SIM initiation started.");

    pthread_mutex_init(&(s->tx.lock), NULL);

    pthread_mutex_init(&(s->gps.lock), NULL);
    s->gps.ready = 0;

    pthread_cond_init(&(s->gps.initialization_done), NULL);

    s->status = 0;
    s->head = 0;
    s->tail = 0;
    s->sample_length = 0;

    pthread_cond_init(&(s->fifo_write_ready), NULL);

    pthread_cond_init(&(s->fifo_read_ready), NULL);

    s->time = 0.0;

    printf("\nGPS SIM initiated.");
}

size_t get_sample_length(sim_t *s)
{
    long length;

    length = s->head - s->tail;
    if (length < 0)
        length += FIFO_LENGTH;

    return ((size_t)length);
}

size_t fifo_read(int16_t *buffer, size_t samples, sim_t *s)
{
    size_t length;
    size_t samples_remaining;
    int16_t *buffer_current = buffer;

    length = get_sample_length(s);

    if (length < samples)
        samples = length;

    length = samples; // return value

    samples_remaining = FIFO_LENGTH - s->tail;

    if (samples > samples_remaining)
    {
        memcpy(buffer_current, &(s->fifo[s->tail * 2]),
               samples_remaining * sizeof(int16_t) * 2);
        s->tail = 0;
        buffer_current += samples_remaining * 2;
        samples -= samples_remaining;
    }

    memcpy(buffer_current, &(s->fifo[s->tail * 2]),
           samples * sizeof(int16_t) * 2);
    s->tail += (long)samples;
    if (s->tail >= FIFO_LENGTH)
        s->tail -= FIFO_LENGTH;

    return (length);
}

bool is_finished_generation(sim_t *s) { return s->finished; }

int is_fifo_write_ready(sim_t *s)
{
    int status = 0;

    s->sample_length = get_sample_length(s);
    if (s->sample_length < NUM_IQ_SAMPLES)
        status = 1;

    return (status);
}

void *tx_task(void *arg)
{
    sim_t *s = (sim_t *)arg;
    size_t samples_populated;
    size_t num_samps_sent = 0;
    size_t samples_per_buffer = SAMPLES_PER_BUFFER;
    while (1)
    {
        if (stop_signal_called)
            goto out;

        int16_t *tx_buffer_current = s->tx.buffer;
        unsigned int buffer_samples_remaining = SAMPLES_PER_BUFFER;
        const void **buffs_ptr = NULL;

        while (buffer_samples_remaining > 0)
        {
            pthread_mutex_lock(&(s->gps.lock));

            while (get_sample_length(s) == 0)
            {
                pthread_cond_wait(&(s->fifo_read_ready), &(s->gps.lock));
            }

            samples_populated = fifo_read(tx_buffer_current, buffer_samples_remaining, s);
            pthread_mutex_unlock(&(s->gps.lock));

            pthread_cond_signal(&(s->fifo_write_ready));

            buffer_samples_remaining -= (unsigned int)samples_populated;

            tx_buffer_current += (2 * samples_populated);
        }
        buffs_ptr = (const void **)tx_buffer_current;
        uhd_tx_streamer_send(s->tx.stream, s->tx.buffer_ptr, SAMPLES_PER_BUFFER, &s->tx.md, 1000, &samples_populated);

        if (is_fifo_write_ready(s))
        {
            fprintf(stderr, "\rTime = %4.1f", s->time);
            s->time += 0.1;
            fflush(stdout);
        }
        else if (is_finished_generation(s))
        {
            goto out;
        }
    }
out:
    return NULL;
}

int start_tx_task(sim_t *s)
{
    int status;

    status = pthread_create(&(s->tx.thread), NULL, tx_task, s);

    return (status);
}

int start_gps_task(sim_t *s)
{
    int status;

    status = pthread_create(&(s->gps.thread), NULL, gps_task, s);

    return (status);
}

void usage(char *progname)
{
    printf(
        "Usage: %s [options]\n"
        "Options:\n"
        "  -e <gps_nav>     RINEX navigation file for GPS ephemerides "
        "(required)\n"
        "  -u <user_motion> User motion file (dynamic mode)\n"
        "  -g <nmea_gga>    NMEA GGA stream (dynamic mode)\n"
        "  -l <location>    Lat,Lon,Hgt (static mode) e.g. 35.274,137.014,100\n"
        "  -t <date,time>   Scenario start time YYYY/MM/DD,hh:mm:ss\n"
        "  -T <date,time>   Overwrite TOC and TOE to scenario start time\n"
        "  -d <duration>    Duration [sec] (max: %.0f)\n"
        "  -a <rf_gain>     Absolute RF gain in [0 ... 60] (default: 30)\n"
        "  -I               Disable ionospheric delay for spacecraft scenario\n",
        progname,

        ((double)USER_MOTION_SIZE) / 10.0);

    return;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        usage(argv[0]);
        exit(1);
    }

    // Set default values
    sim_t s;

    s.finished = false;
    s.opt.navfile[0] = 0;
    s.opt.umfile[0] = 0;
    s.opt.g0.week = -1;
    s.opt.g0.sec = 0.0;
    s.opt.iduration = USER_MOTION_SIZE;
    s.opt.verb = TRUE;
    s.opt.nmeaGGA = FALSE;
    s.opt.staticLocationMode = TRUE;
    s.opt.llh[0] = 42.3601;
    s.opt.llh[1] = -71.0589;
    s.opt.llh[2] = 2;
    s.opt.interactive = FALSE;
    s.opt.timeoverwrite = FALSE;
    s.opt.iono_enable = TRUE;
    s.udp_port = 5671;
    // Options
    int result;
    double duration;
    datetime_t t0;
    double gain = 45;
    char *device_args = NULL;
    size_t channel = 1;
    bool verbose = false;
    int return_code = EXIT_SUCCESS;
    char error_string[512];
    double rate = TX_SAMPLERATE;
    double freq = TX_FREQUENCY;

    // Buffer sizes
    size_t samps_per_buff = SAMPLES_PER_BUFFER;
    int16_t *buffer = NULL;
    const void **buffer_ptr = NULL;

    while ((result = getopt(argc, argv, "e:u:g:l:T:t:d:G:a:p:iI")) != -1)
    {
        switch (result)
        {
        case 'e':
            strcpy(s.opt.navfile, optarg);
            break;

        case 'u':
            strcpy(s.opt.umfile, optarg);
            s.opt.nmeaGGA = FALSE;
            s.opt.staticLocationMode = FALSE;
            break;

        case 'g':
            strcpy(s.opt.umfile, optarg);
            s.opt.nmeaGGA = TRUE;
            s.opt.staticLocationMode = FALSE;
            break;

        case 'l':
            // Static geodetic coordinates input mode
            // Added by scateu@gmail.com
            s.opt.nmeaGGA = FALSE;
            s.opt.staticLocationMode = TRUE;
            sscanf(optarg, "%lf,%lf,%lf", &s.opt.llh[0], &s.opt.llh[1],
                   &s.opt.llh[2]);
            break;

        case 'T':
            s.opt.timeoverwrite = TRUE;
            if (strncmp(optarg, "now", 3) == 0)
            {
                time_t timer;
                struct tm *gmt;

                time(&timer);
                gmt = gmtime(&timer);

                t0.y = gmt->tm_year + 1900;
                t0.m = gmt->tm_mon + 1;
                t0.d = gmt->tm_mday;
                t0.hh = gmt->tm_hour;
                t0.mm = gmt->tm_min;
                t0.sec = (double)gmt->tm_sec;

                date2gps(&t0, &s.opt.g0);

                break;
            }

        case 't':
            sscanf(optarg, "%d/%d/%d,%d:%d:%lf", &t0.y, &t0.m, &t0.d, &t0.hh, &t0.mm,
                   &t0.sec);
            if (t0.y <= 1980 || t0.m < 1 || t0.m > 12 || t0.d < 1 || t0.d > 31 ||
                t0.hh < 0 || t0.hh > 23 || t0.mm < 0 || t0.mm > 59 || t0.sec < 0.0 ||
                t0.sec >= 60.0)
            {
                printf("ERROR: Invalid date and time.\n");
                exit(1);
            }
            t0.sec = floor(t0.sec);
            date2gps(&t0, &s.opt.g0);
            break;

        case 'd':
            duration = atof(optarg);
            if (duration < 0.0 || duration > ((double)USER_MOTION_SIZE) / 10.0)
            {
                printf("ERROR: Invalid duration.\n");
                exit(1);
            }
            s.opt.iduration = (int)(duration * 10.0 + 0.5);
            break;

        case 'G':
            gain = atof(optarg);
            if (gain < 0)
                gain = 0;
            if (gain > 60)
                gain = 60;
            break;

        case 'a':
            device_args = strdup(optarg);
            break;

        case 'p':
            s.udp_port = atoi(optarg);
            
            break;

        case 'i':
            s.opt.interactive = TRUE;
            break;

        case 'I':
            s.opt.iono_enable = FALSE; // Disable ionospheric correction
            break;

        case ':':

        case '?':
            usage(argv[0]);
            exit(1);

        default:
            break;
        }
    }

    if (s.opt.navfile[0] == 0)
    {
        printf("ERROR: GPS ephemeris file is not specified.\n");
        exit(1);
    }

    if (s.opt.umfile[0] == 0 && !s.opt.staticLocationMode)
    {
        printf("ERROR: User motion file / NMEA GGA stream is not specified.\n");
        printf("You may use -l to specify the static location directly.\n");
        exit(1);
    }

    // Setup UHD params
    if (!device_args)
        device_args = strdup("");

    // Initialize simulator
    init_sim(&s);

    // Allocate FIFOs to hold 0.1 seconds of I/Q samples each.
    s.fifo = (int16_t *)malloc(FIFO_LENGTH * sizeof(int16_t) *
                               2); // for 16-bit I and Q samples

    if (s.fifo == NULL)
    {
        printf("ERROR: Failed to allocate I/Q sample buffer.\n");
        goto out;
    }

    // Create USRP
    uhd_usrp_handle usrp;
    fprintf(stderr, "Creating USRP with args \"%s\"...\n", device_args);
    EXECUTE_OR_GOTO(free_option_strings, uhd_usrp_make(&usrp, device_args))

    // Create TX streamer
    uhd_tx_streamer_handle tx_streamer;
    EXECUTE_OR_GOTO(free_usrp, uhd_tx_streamer_make(&tx_streamer))

    // Create TX metadata
    uhd_tx_metadata_handle md;
    EXECUTE_OR_GOTO(free_tx_streamer,
                    uhd_tx_metadata_make(&md, false, 0, 0.1, true, false))

    // Create other necessary structs
    uhd_tune_request_t tune_request = {
        .target_freq = TX_FREQUENCY,
        .rf_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO,
        .dsp_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO};
    uhd_tune_result_t tune_result;

    uhd_stream_args_t stream_args = {.cpu_format = "sc16",
                                     .otw_format = "sc16",
                                     .args = "",
                                     .channel_list = &channel,
                                     .n_channels = 1};

    // Initializing device
    // Set rate
    fprintf(stderr, "Setting TX Rate: %f...\n", rate);
    EXECUTE_OR_GOTO(free_tx_metadata, uhd_usrp_set_tx_rate(usrp, rate, channel))

    // See what rate actually is
    EXECUTE_OR_GOTO(free_tx_metadata, uhd_usrp_get_tx_rate(usrp, channel, &rate))
    fprintf(stderr, "Actual TX Rate: %f...\n\n", rate);

    // Set gain
    fprintf(stderr, "Setting TX Gain: %f db...\n", gain);
    EXECUTE_OR_GOTO(free_tx_metadata, uhd_usrp_set_tx_gain(usrp, gain, channel, ""))

    // See what gain actually is
    EXECUTE_OR_GOTO(free_tx_metadata,
                    uhd_usrp_get_tx_gain(usrp, channel, "", &gain))
    fprintf(stderr, "Actual TX Gain: %f...\n", gain);

    // Set frequency
    fprintf(stderr, "Setting TX frequency: %f MHz...\n", freq / 1e6);
    EXECUTE_OR_GOTO(free_tx_metadata, uhd_usrp_set_tx_freq(usrp, &tune_request,
                                                           channel, &tune_result))

    // See what frequency actually is
    EXECUTE_OR_GOTO(free_tx_metadata, uhd_usrp_get_tx_freq(usrp, channel, &freq))
    fprintf(stderr, "Actual TX frequency: %f MHz...\n", freq / 1e6);

    // Set up streamer
    stream_args.channel_list = &channel;
    EXECUTE_OR_GOTO(free_tx_streamer,
                    uhd_usrp_get_tx_stream(usrp, &stream_args, tx_streamer))

    // Set up buffer
    EXECUTE_OR_GOTO(free_tx_streamer,
                    uhd_tx_streamer_max_num_samps(tx_streamer, &samps_per_buff))
    fprintf(stderr, "Buffer size in samples: %ld\n", samps_per_buff);

    s.tx.stream = tx_streamer;
    s.tx.md = md;

    // Allocate TX buffer to hold each block of samples to transmit.
    s.tx.buffer = (int16_t *)malloc(SAMPLES_PER_BUFFER * sizeof(int16_t) * 2); // for 16-bit I and Q samples
    s.tx.buffer_ptr = (const void **)&s.tx.buffer;

    if (s.tx.buffer == NULL)
    {
        fprintf(stderr, "ERROR: Failed to allocate TX buffer.\n");
        goto out;
    }

    size_t i = 0;
    for (i = 0; i < (samps_per_buff * 2); i += 2)
    {
        s.tx.buffer[i] = 0;
        s.tx.buffer[i + 1] = 0;
    }

    // Ctrl+C will exit loop
    signal(SIGINT, &sigint_handler);
    fprintf(stderr, "Press Ctrl+C to stop streaming...\n");

    fprintf(stderr, "Opening and initializing USRP...\n");

    // Start GPS task.
    s.status = start_gps_task(&s);
    if (s.status < 0)
    {
        fprintf(stderr, "Failed to start GPS task.\n");
        goto out;
    }
    else
        printf("Creating GPS task...\n");

    // Wait until GPS task is initialized
    pthread_mutex_lock(&(s.tx.lock));
    while (!s.gps.ready)
        pthread_cond_wait(&(s.gps.initialization_done), &(s.tx.lock));
    pthread_mutex_unlock(&(s.tx.lock));

    // Fillfull the FIFO.
    if (is_fifo_write_ready(&s))
        pthread_cond_signal(&(s.fifo_write_ready));

    // Start TX task
    s.status = start_tx_task(&s);

    if (s.status < 0)
    {
        fprintf(stderr, "Failed to start TX task.\n");
        goto out;
    }
    else
        printf("Creating TX task...\n");

    // Running...
    printf("Running...\n"
           "Press Ctrl+C to abort.\n");

    // Wainting for TX task to complete.
    pthread_join(s.tx.thread, NULL);
    printf("\nDone!\n");

out:
    // Disable TX module and shut down underlying TX stream.
    uhd_tx_streamer_free(&s.tx.stream);
    uhd_tx_metadata_free(&md);
    uhd_usrp_free(&usrp);
    free(device_args);

    // Free up resources
    if (s.tx.buffer != NULL)
        free(s.tx.buffer);

    if (s.fifo != NULL)
        free(s.fifo);

    return EXIT_SUCCESS;

free_buff:
    free(&s.tx.buffer);

free_tx_streamer:
    if (verbose)
    {
        fprintf(stderr, "Cleaning up TX streamer.\n");
    }
    uhd_tx_streamer_free(&s.tx.stream);

free_tx_metadata:
    if (verbose)
    {
        fprintf(stderr, "Cleaning up TX metadata.\n");
    }
    uhd_tx_metadata_free(&md);

free_usrp:
    if (verbose)
    {
        fprintf(stderr, "Cleaning up USRP.\n");
    }
    if (return_code != EXIT_SUCCESS && usrp != NULL)
    {
        uhd_usrp_last_error(usrp, error_string, 512);
        fprintf(stderr, "USRP reported the following error: %s\n", error_string);
    }
    uhd_usrp_free(&usrp);

free_option_strings:
    if (device_args)
        free(device_args);

    fprintf(stderr, (return_code ? "Failure\n" : "Success\n"));

    return return_code;
}
