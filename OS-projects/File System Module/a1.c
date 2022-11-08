#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include  <errno.h>
#include <stdint.h>
#define __DEBUG

#ifdef __DEBUG
void debug_info (const char *file, const char *function, const int line)
{
        fprintf(stderr, "DEBUG. ERROR PLACE: File=\"%s\", Function=\"%s\", Line=\"%d\"\n", file, function, line);
}

#define ERR_MSG(DBG_MSG) { \
        perror(DBG_MSG); \
        debug_info(__FILE__, __FUNCTION__, __LINE__); \
}

#else                   // with no __DEBUG just displays the error message

#define ERR_MSG(DBG_MSG) { \
        perror(DBG_MSG); \
}

#endif

#define MAX_PATH_LEN 257

#pragma pack(push,1)
typedef struct header {     
    uint16_t header_size;
    char magic;
} header;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct header_part2 {  
    uint8_t version;
    uint8_t no_of_sections;
} header_part2;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct section {  
    char sect_name[6];
    uint16_t sect_type;
    uint32_t sect_offset;
    uint32_t sect_size;
} section;
#pragma pack(pop)


int open_file(char *path) {
    int fd = open(path, O_RDONLY);
    
    if (fd < 0) {
        ERR_MSG ("ERROR\nCould not open file");
        return -1;
    }
    return fd;
}

section *s;
uint8_t nr_of_sections;
bool print_parse = true;
bool used_by_findall = false;
bool parse(char *path) {
    int fd = open_file(path);
    if(fd == -1)
        exit(4);

    if (fd < 0) {
        ERR_MSG ("ERROR\nCould not open file");
        exit(4);
    }

    header *h = (header*)malloc(sizeof(header));
    lseek(fd, -sizeof(header), SEEK_END);

    read(fd, h, sizeof(header));
    lseek(fd, 0, SEEK_CUR);

    lseek(fd, -(h->header_size), SEEK_END);
    header_part2 *h2 = (header_part2*)malloc(sizeof(header_part2));
    read(fd, h2, sizeof(header_part2));

    s = (section*)malloc(h2->no_of_sections * sizeof(section));
    read(fd, s, h2->no_of_sections*sizeof(section));

    nr_of_sections = h2->no_of_sections;
    //sections = s;

    

    for(int i=0; i<h2->no_of_sections; i++) {
        
        if(h->magic != 'y') {
            free(h);
            free(h2);  
            if(used_by_findall)
                return false;
            printf("ERROR\nwrong magic\n");    
            free(s);
            return false;
        }
        else if(h2->version < 42 || h2->version > 90) {
            free(h);
            free(h2); 
            if(used_by_findall)
                return false;
            printf("ERROR\nwrong version\n");     
            free(s);
            return false;
        }
        else if(h2->no_of_sections < 3 || h2->no_of_sections >13) {
            free(h);
            free(h2); 
            if(used_by_findall)
                return false;
            printf("ERROR\nwrong sect_nr\n");    
            free(s);
            return false;
        }
        else if(s[i].sect_type != 61 && s[i].sect_type != 66) {
            free(h);
            free(h2);
            if(used_by_findall)
                return false;
            printf("ERROR\nwrong sect_types\n");      
            free(s);
            return false;
        }
    }

    if(!print_parse)
        goto alfa;

    printf("SUCCESS\nversion=%d\nnr_sections=%d\n",h2->version, h2->no_of_sections);
    for(int i=0; i<h2->no_of_sections; i++) {
        printf("section%d: ",i+1);
        for(int j = 0; j < 6; j++)
            if(s[i].sect_name[j] != '\x00')
                printf("%c", s[i].sect_name[j]);
        printf(" %d %d\n",s[i].sect_type,s[i].sect_size);
    }
    alfa:
    free(h);
    free(h2);
    if(used_by_findall)
        return true;
    
    free(s);
    return true;
}

bool has_enough_sections(char *path) {
    int fd = open_file(path);
    if(fd == -1) {
        printf("ERROR\ninvalid file");
        free(s);
        exit(4);
    }
        
    print_parse = false;
    if(!parse(path)) {
        free(s);
        return false;
    }  
    print_parse = true;

    if(nr_of_sections < 4) {
        free(s);
        return false;
    }
    
    int good_sections = 0;
    for(int j = 0; j < nr_of_sections; j++) {
        uint32_t size = s[j].sect_size, offset = s[j].sect_offset;

        lseek(fd, offset, SEEK_SET);
        char *section_content = (char*)malloc(size);
        read(fd, section_content, size);

        uint32_t i = 0;
        int current_line = 1;
        while(i < size) {
            if(section_content[i] == '\n') {
                current_line++;
            }
            i++;
        }
        free(section_content);
        if(current_line == 16)
            good_sections++;
        if(good_sections == 4) {
            free(s);
            return true;
        }
            
    }
    free(s);
    return false;
}

void listDir(char *dirName, int size_greater, char *name_starts_with, bool recursive)
{
    DIR* dir;
    struct dirent *dirEntry;
    struct stat inode;
    char name[MAX_PATH_LEN];

    dir = opendir(dirName);
    if (dir == 0) {
        ERR_MSG ("ERROR\nCould not open directory");
        exit(4);
    }

    // iterate the directory contents
    while ((dirEntry=readdir(dir)) != 0) {
        
        // build the complete path to the element in the directory
        snprintf(name, MAX_PATH_LEN, "%s/%s",dirName,dirEntry->d_name);
        
        // get info about the directory's element
        lstat (name, &inode);

        //ignore refferances to current and parent directory
        if(strcmp(dirEntry->d_name,".") == 0 || strcmp(dirEntry->d_name,"..") == 0)
            continue;

        //compare with the start name characters
        if(name_starts_with != NULL) {

            int size_to_compare = strlen(name_starts_with);
            if(strncmp(name_starts_with, dirEntry->d_name, size_to_compare) != 0) {
                continue;
            }
        }

        // test the type of the directory's element
        if (S_ISDIR(inode.st_mode)) {
            //traverse directory tree
            if(recursive) {
                listDir(name, size_greater, name_starts_with, true);
            }
            
        }
        else 
            if (S_ISREG(inode.st_mode)) {
                if(inode.st_size < size_greater) 
                    continue;
                if( ( used_by_findall && has_enough_sections(name) ) || size_greater != 0) {
                    printf("%s/%s\n",dirName, dirEntry->d_name);
                }
               
            }
        if(!used_by_findall && size_greater == 0)
            printf("%s/%s\n",dirName, dirEntry->d_name);
    }

    closedir(dir);
}

int to_int(char string[]) {
    int size = strlen(string);
    int to_return = 0, power_of_ten = 1;

    for(int i= size - 1; i >= 0; i--) {
        to_return += power_of_ten*(string[i] - '0');
        power_of_ten *= 10;
    }

    if(to_return < 0)
        to_return = 0;

    return to_return;
}

void print_line_backwards(char *section_content, int i, uint32_t size) {
    if(section_content[i] != '\n' && i < size) {
        print_line_backwards(section_content, i+1, size);
        printf("%c", section_content[i]);
    }
}

void extract(char* path, int line, uint8_t section) {
    int fd = open_file(path);
    if(fd == -1) {
        printf("ERROR\ninvalid file");
        exit(4);
    }
        

    print_parse = false;
    parse(path);
    print_parse = true;

    if(section < 1 || nr_of_sections < section) {
        ERR_MSG("ERROR\ninvalid section");
        exit(4);
    }

    uint32_t size = s[section - 1].sect_size, offset = s[section - 1].sect_offset;

    lseek(fd, offset, SEEK_SET);
    char *section_content = (char*)malloc(size);
    read(fd, section_content, size);

    uint32_t i = 0;
    int current_line = 1;
    while(i < size) {
        if(line == current_line) {
            printf("SUCCESS\n");
            print_line_backwards(section_content, i, size);
            printf("\n");
            free(section_content);
            return;
        }
        else if(section_content[i] == '\n') {
            current_line++;
        }
        i++;
    }
    printf("ERROR\ninvalid line");
    printf("\n");
}

void findall(char *path) {
    int fd = open_file(path);
    if(fd == -1) {
        printf("ERROR\ninvalid directory path");
        exit(4);
    }
    printf("SUCCESS\n");

    used_by_findall = true;
    print_parse = false;
    listDir(path, 0, NULL, true);
    used_by_findall = false;
    print_parse = true;
}

int main(int argc, char **argv)
{
     if(argc >= 2){
        if(strcmp(argv[1], "variant") == 0){
            printf("32166\n");
        }
        else if(strcmp(argv[1], "list") == 0 && argc > 2) {
            
            bool recursive = false;
            int size_greater = 0;
            char *name_starts_with = NULL, *path;

            for(int i= 2; i < argc; i++) {

                if(strcmp(argv[i], "recursive") == 0)
                    recursive = true;
                else {
                    char *substring = strtok(argv[i], "=");

                    if(strcmp(substring, "path") == 0) {
                        path = strtok(NULL, "=");
                    }
                    else if(strcmp(substring, "size_greater") == 0) {
                        size_greater = to_int(strtok(NULL, "="));
                    }
                    else if(strcmp(substring, "name_starts_with") == 0) {
                        name_starts_with = strtok(NULL, "=");
                    }
                }
            }
             
            struct stat fileMetadata;
            if (stat(path, &fileMetadata) < 0) {  // get info 
                ERR_MSG("ERROR (getting info about the file)");
                exit(2);
            }
    
            if (S_ISDIR(fileMetadata.st_mode)) { // it is a directory
                // list directory's contents
                printf("SUCCESS\n");
                listDir(path, size_greater, name_starts_with, recursive);
            } else {
                printf("ERROR\n%s is not a directory!\n", path);
                exit(3);
            }
            free(name_starts_with);
        }
        else if(strcmp(argv[1], "parse") == 0 && argc > 2) {
            char *substring = strtok(argv[2], "=");

            if(strcmp(substring,"path") != 0) {
                ERR_MSG("ERROR no path specified\n");
                exit(4);
            }

            char *path;

            path = strtok(NULL, "=");

            parse(path);
        }
        else if(strcmp(argv[1], "extract") == 0 && argc > 4) {
            char *path;
            int line;
            uint8_t section;

            for(int i= 2; i < argc; i++) {
                char *substring = strtok(argv[i], "=");

                if(strcmp(substring, "path") == 0) {
                    path = strtok(NULL, "=");
                }
                else if(strcmp(substring, "section") == 0) {
                    section = (uint8_t)to_int(strtok(NULL, "="));
                    }
                else if(strcmp(substring, "line") == 0) {
                    line = to_int(strtok(NULL, "="));
                }
            }

            extract(path, line, section);
        }
        else if(strcmp(argv[1], "findall") == 0 && argc == 3) {
            char *substring = strtok(argv[2], "=");

            if(strcmp(substring,"path") != 0) {
                ERR_MSG("ERROR no path specified\n");
                exit(4);
            }

            char *path;

            path = strtok(NULL, "=");

            findall(path);
        }
    }
    else {
        printf("No arguments\n");
        exit(1);
    }
    
    return 0;
}