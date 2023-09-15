#include<stdio.h>
#include<string.h>
#include<stdbool.h>

#define MAXCHAR 1000

int main(){

    FILE *fp_csv;
    char row[MAXCHAR];

    fp_csv = fopen("sample.csv","r");

    ;

    while (feof(fp_csv) != true)
    {
        fgets(row, MAXCHAR, fp_csv);
        printf("Row: %s", row);
    }
    

    return 0;
}