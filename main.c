#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>

void searchForFiles(const char* dir);
void createHeaderFile(const char* fullName, const char* fullNameHeader, int headerIsNew);

int main(int argc, char** args)
{
    const char* startDir;
    
    if(argc > 1)
    {
        startDir = args[1];
    }
    else
    {
        startDir = ".";
    }
    
    printf("WARNING: Do you want all header files under `%s` to be overwritten? If so, hit ENTER\n", startDir);
    
    while(getchar() != '\n'){}
    
    printf("Watching for C files to be saved in %s\n", startDir);
    
    while(1)
    {
        sleep(1);
        
        searchForFiles(startDir);
    }
    
    return 0;
}

int isDir(const char* dir)
{
    struct stat dirStat;
    
    stat(dir, &dirStat);
    
    return S_ISDIR(dirStat.st_mode);
}

time_t getFileLastModified(const char* file)
{
    struct stat fileStat;
    
    stat(file, &fileStat);
    
    return fileStat.st_mtime;
}

void searchForFiles(const char* dir)
{
    struct dirent** files = NULL;
    int count = scandir(dir, &files, NULL, alphasort);
    
    for(int i = 0; i < count; i++)
    {
        // Skip shortcuts for this dir & parent dir
        // Also skip any file named `main.c`
        if(files[i]->d_name[0] == '.' || strcmp(files[i]->d_name, "main.c") == 0)
        {
            continue;
        }
        
        // Get the full path of the file
        char* fullName = malloc(sizeof(char) * (strlen(files[i]->d_name) + strlen(dir) + 2));
        sprintf(fullName, "%s/%s", dir, files[i]->d_name);
        int fullNameLength = strlen(fullName);
        int headerIsNew = 0;
        
        if(isDir(fullName))
        {
            searchForFiles(fullName);
        }
        else if(fullName[fullNameLength - 2] == '.' && fullName[fullNameLength - 1] == 'c')
        {
            char* fullNameHeader = malloc(sizeof(char) * (fullNameLength + 1));
            sprintf(fullNameHeader, "%s", fullName);
            fullNameHeader[fullNameLength - 1] = 'h';
            
            int updateHeader = 0;
            
            // If header doesn't exist
            if(access(fullNameHeader, F_OK) == -1)
            {
                updateHeader = 1;
                headerIsNew = 1;
            }
            else if(getFileLastModified(fullName) > getFileLastModified(fullNameHeader))
            {
                updateHeader = 1;
            }
            
            if(updateHeader)
            {
                printf("Updating header for %s...\n", fullNameHeader);
                
                createHeaderFile(fullName, fullNameHeader, headerIsNew);
            }
            
            free(fullNameHeader);
        }
        
        free(fullName);
    }
    
    free(files);
}

void createHeaderFile(const char* fullName, const char* fullNameHeader, int headerIsNew)
{
    FILE* file = NULL;
    FILE* header = NULL;
    char* line = NULL;
    size_t length = 0;
    ssize_t read;
    int toBreak;
    
    file = fopen(fullName, "r");
    if(file == NULL)
    {
        printf("Could not read from %s!\n", fullName);
        
        return;
    }
    
    char* prefix = NULL;
    
    if(headerIsNew)
    {
        prefix = "#pragma once\n\n";
        printf("test\n");
    }
    else
    {
        // 100k characters should be enough, right? lol
        int prefixMaxLength = 100 * 1000;
        prefix = malloc(sizeof(char) * prefixMaxLength);
        int prefixSoFar = 0;
        
        FILE* headerR = fopen(fullNameHeader, "r");
        if(headerR == NULL)
        {
            printf("Could not write to %s!\n", fullNameHeader);
            
            return;
        }
        
        while((read = getline(&line, &length, headerR)) != -1)
        {
            if(line[0] != ' ' && line[0] != '\n' && line[0] != '\r' && line[0] != '#')
            {
                break;
            }
            
            memcpy(&prefix[prefixSoFar], line, sizeof(char) * (strlen(line)));
            
            prefixSoFar += strlen(line);
        }
        
        fclose(headerR);
        
        prefix[prefixSoFar] = '\0';
    }
    
    header = fopen(fullNameHeader, "w");
    if(header == NULL)
    {
        printf("Could not write to %s!\n", fullNameHeader);
        
        return;
    }
    
    fputs(prefix, header);
    
    if(!headerIsNew)
    {
        free(prefix);
    }
    
    int writingStreak = 0;
    while((read = getline(&line, &length, file)) != -1)
    {
        int toBreak = 0;
        int lineLastChar = -1;
        
        if(!writingStreak && (line[0] == ' ' || line[0] == '\t' || line[0] == '\n' || line[0] == '\r' || line[0] == '{' || line[0] == '}' || line[0] == '#'))
        {
            continue;
        }
        
        if(writingStreak && line[0] == '{')
        {
            writingStreak = 0;
            
            continue;
        }
        
        for(int i = strlen(line) - 1; i > 1; i--)
        {
            if(line[i] == '\0' || line[i] == '\r' || line[i] == '\n')
            {
                continue;
            }
            
            if(lineLastChar == -1)
            {
                lineLastChar = i;
            }
            
            if(line[i] == ';')
            {
                toBreak = 1;
                
                break;
            }
        }
        
        if(toBreak)
        {
            continue;
        }
        
        int stopWritingStreak = 0;
        if(line[lineLastChar] == '(')
        {
            writingStreak = 1;
        }
        else if(line[lineLastChar + 1] == ')')
        {
            lineLastChar++;
            writingStreak = 0;
            stopWritingStreak = 1;
        }
        
        char* lineFull = malloc(sizeof(char) * (lineLastChar + 4));
        memcpy(lineFull, line, sizeof(char) * (lineLastChar + 1));
        if(writingStreak)
        {
            lineFull[lineLastChar + 1] = '\n';
            lineFull[lineLastChar + 2] = '\0';
        }
        else
        {
            lineFull[lineLastChar + 1] = ';';
            lineFull[lineLastChar + 2] = '\n';
            lineFull[lineLastChar + 3] = '\0';
        }
        fputs(lineFull, header);
        free(lineFull);
        
        if(!stopWritingStreak)
        {
            writingStreak = 1;
        }
    }
    
    fclose(file);
    fclose(header);
}
