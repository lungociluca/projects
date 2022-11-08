#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>
#include <sys/time.h>
#include <sys/mman.h>
#include  <errno.h>
#include <stdint.h>

#define PIPE1 "RESP_PIPE_32166"
#define PIPE2 "REQ_PIPE_32166"
#define SHM "/0LVW2fH"
#define READ 0
#define WRITE 1

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

int p_read;
int p_write;

u_int8_t size_of_path = 0;
char* pipe_operation(int operation, char string[]) {
    if(operation == READ) {
        u_int8_t nr;
        read(p_read, &nr, 1);
        size_of_path = nr;
        char *data = (char*)malloc(nr);
        read(p_read, data, nr);
        return data;
    }
    else if(operation == WRITE) {
        u_int8_t nr = strlen(string);
        write(p_write, &nr, 1);
        write(p_write, string, nr);
        return NULL;
    }
    printf("Wrong value for 'operation' argument\n");
    return NULL;
}

unsigned int read_number() {
    unsigned int to_return;
    read(p_read, &to_return, 4);
    return to_return;
}

bool compare_with_string(char *data, char to_compare[]) {
    int len = strlen(to_compare);
    for(int i = 0; i < len; i++) {
        if(data[i] != to_compare[i])
            return false;
    }
    return true;
}

int main()
{   
    unlink(PIPE1);
    const unsigned int nr = 32166;
    unsigned int shm_size;
    int shm;

    if(mkfifo(PIPE1, 0640 | S_IFIFO) == -1) {
        printf("ERROR\ncannot create the response pipe\n");
        exit(1);
    }
    
    p_read = open(PIPE2, O_RDONLY);
    if(p_read < 0) {
        printf("ERROR\ncannot open the request pipe\n");
        exit(1);
    }

    p_write = open(PIPE1, O_WRONLY);
    if(p_write < 0) {
        printf("ERROR\ncannot open the response pipe\n");
        exit(1);
    }


    pipe_operation(WRITE, "CONNECT");
    printf("SUCCESS\n");

    char *data;
    char *maped_file;
    char *maped_shm;
    int file_size = 0;

    do {
        data = pipe_operation(READ, "");

        if(compare_with_string(data, "PING")){
            pipe_operation(WRITE, "PING");
            pipe_operation(WRITE, "PONG");
            write(p_write, &nr, 4);
        }        
        else if(compare_with_string(data, "CREATE_SHM")) {
            shm_unlink(SHM);
            shm_size = read_number();

            shm = shm_open(SHM, O_CREAT | O_RDWR, 0664);
            if(shm < 0) {
                printf("CREATE_SHM ERROR\n");
                exit(1);
            }

            if(ftruncate(shm, shm_size) == -1) {
                printf("Cannot allocate memory for the shm\n");
                exit(1);
            }

            pipe_operation(WRITE, "CREATE_SHM");
            maped_shm = (char*)mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
            if(maped_shm == MAP_FAILED) {
                pipe_operation(WRITE, "ERROR");
                printf("Can't create\n");
                exit(1);
            }
            pipe_operation(WRITE, "SUCCESS");
        }
        else if(compare_with_string(data, "WRITE_TO_SHM")) {
            unsigned int offset = read_number();
            unsigned int value = read_number();
            pipe_operation(WRITE, "WRITE_TO_SHM");

            printf("shm: offset is %d, nr %d, shm size is %d\n",offset, value, shm_size);
            if(offset < 0 || offset >= shm_size - 4) {
                error:
                printf("Error while writting in the shm.\n");
                pipe_operation(WRITE, "ERROR");
                continue;
            }

            printf("lseek => %ld\n" ,lseek(shm, offset, SEEK_SET));
            if(write(shm, &value, 4) < 0)
                goto error;

            pipe_operation(WRITE, "SUCCESS");
        }
        else if(compare_with_string(data, "MAP_FILE")) {
            char *file_name = pipe_operation(READ, "");
            file_name[size_of_path] = '\0';

            int fd = open(file_name, O_RDONLY);
            if(fd <= 0) {
                printf("Fd is negative\n");
                goto error_map;
            }

            struct stat fileMetadata;
            stat(file_name, &fileMetadata);
            file_size = fileMetadata.st_size;
            printf("Size of file is %d\n", file_size);

            
            maped_file = (char*)mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
            if(maped_file == MAP_FAILED) {
                error_map:
                pipe_operation(WRITE, "MAP_FILE");
                pipe_operation(WRITE, "ERROR");
                printf("Can't map file\n");
                exit(1);
            }
            pipe_operation(WRITE, "MAP_FILE");
            pipe_operation(WRITE, "SUCCESS");
        }
        else if(compare_with_string(data, "READ_FROM_FILE_OFFSET")) {
            
            unsigned int offset = read_number();
            unsigned int no_of_bytes = read_number();

            if(offset + no_of_bytes >= file_size) {
                printf("Offset is %d, nr of bytes is %d, file size is %d\n", offset, no_of_bytes, file_size);
                pipe_operation(WRITE, "READ_FROM_FILE_OFFSET");
                pipe_operation(WRITE, "ERROR");
            }
            else {
                for(int i = 0; i < no_of_bytes; i++) {
                    maped_shm[i] = maped_file[i + offset];
                }

                pipe_operation(WRITE, "READ_FROM_FILE_OFFSET");
                pipe_operation(WRITE, "SUCCESS");
            }
        }
        else if(compare_with_string(data, "READ_FROM_FILE_SECTION")) {
            unsigned int section_no = read_number();
            unsigned int offset = read_number();
            unsigned int no_of_bytes = read_number();

            header *h = (header*)malloc(sizeof(header));

            h = (header*)(maped_file + file_size - sizeof(header));
            printf("Header size is %d, ,magic is %c\n",h->header_size,h->magic);
        

            printf("Size of the sections is %ld, size of a section is %ld\n", h->header_size - sizeof(header_part2) - sizeof(header), sizeof(section));
            int nr_of_sections =( h->header_size - sizeof(header_part2) - sizeof(header) ) / sizeof(section);

            section* all_sections_array = (section*)malloc(nr_of_sections*sizeof(section));
            all_sections_array =(section*) (maped_file + (file_size - sizeof(header) - nr_of_sections * sizeof(section)));

            for(int i = 0; i < nr_of_sections; i++) {
                printf("type %d\n", all_sections_array[i].sect_type);
            }


            pipe_operation(WRITE, "READ_FROM_FILE_SECTION");
            if(section_no > nr_of_sections){
                printf("Such section does not exist\n");
                pipe_operation(WRITE, "ERROR");
            }
            else {
                printf("Starts copying bytes\n");
                u_int32_t offset_in_section = all_sections_array[section_no - 1].sect_offset + offset;
                
                for(int i = 0; i < no_of_bytes; i++) {
                    maped_shm[i] = maped_file[offset_in_section + i];
                }
                pipe_operation(WRITE, "SUCCESS");
            }
        }
        else if(compare_with_string(data, "READ_FROM_LOGICAL_SPACE_OFFSET")) {
            return 0;
        }

    }while(!compare_with_string(data, "EXIT"));
    

    return 0;
}