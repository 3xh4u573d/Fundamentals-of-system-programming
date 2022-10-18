#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <regex.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "plugin_api.h"

static char *g_lib_name = "libmvnN3248.so";
static char *g_plugin_purpose = "File search ipv6 address in text form";
static char *g_plugin_author = "Nazarov Maxim";

#define IPV6_STR "ipv6-addr"

static struct plugin_option g_po_arr[] = {
        {
                {
                        IPV6_STR,
                        required_argument,
                        0, 0,
                },
                "Target ipv6 address"
        }
};
int is_valid_addr(char *);
static int g_po_arr_len = sizeof(g_po_arr)/sizeof(g_po_arr[0]);

int plugin_get_info(struct plugin_info* ppi) {

    if (!ppi) {
        fprintf(stderr, "ERROR: invalid argument\n");
        return -1;
    }

    ppi->plugin_purpose = g_plugin_purpose;
    ppi->plugin_author = g_plugin_author;
    ppi->sup_opts_len = g_po_arr_len;
    ppi->sup_opts = g_po_arr;

    return 0;
}

int plugin_process_file(const char *fname, struct option in_opts[], size_t in_opts_len) {

    int ret = -1;
    unsigned char *ptr = NULL;
    char *DEBUG = getenv("LAB1DEBUG");
    FILE* fp = NULL;

    if (!fname || !in_opts || !in_opts_len) {
        errno = EINVAL;
        return -1;
    }

    if (DEBUG) {
        for (size_t i = 0; i < in_opts_len; ++i)
            fprintf(stderr, "DEBUG: %s: Got option '%s' with arg '%s'\n", g_lib_name, in_opts[i].name, (char*)in_opts[i].flag);
    }
    char* ipv6_addr_input=NULL;
    int got_ipv6_addr_bin = 0;

    // проверка опции
    for (size_t i = 0; i < in_opts_len; ++i) {
        if (!strcmp(in_opts[i].name, IPV6_STR) && got_ipv6_addr_bin == 0) {
            got_ipv6_addr_bin = 1;
            ipv6_addr_input = (char*)in_opts[i].flag;
        }
        else if (!strcmp(in_opts[i].name, IPV6_STR) && got_ipv6_addr_bin == 1){
            if (DEBUG)
                fprintf(stderr, "DEBUG: %s: option '%s' was already supplied\n", g_lib_name, in_opts[i].name);
            errno = EINVAL;
            return -1;
        }
    }

    // проверка адреса
    if (is_valid_addr(ipv6_addr_input) != 0) {
        if (DEBUG) {
            fprintf(stderr, "DEBUG: %s: invalid ipv6_addr\n", g_lib_name);
        }
        errno = EINVAL;
        return -1;
    }


    if (DEBUG) {
        fprintf(stderr, "DEBUG: %s: input: ipv6_addr = %s", g_lib_name, ipv6_addr_input);
    	       }

    int saved_errno = 0;

    // проверка размера файла
    int fd = open(fname, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    struct stat st = {0};
    int res = fstat(fd, &st);
    if (res < 0) {
        saved_errno = errno;
        goto END;
    }

    if (st.st_size == 0) {
        if (DEBUG) {
            fprintf(stderr, "DEBUG: %s: File size should be > 0\n", g_lib_name);
        }
        saved_errno = ERANGE;
        goto END;
    }

    
    fp = fopen(fname, "r");
    if (fp == NULL) {
        exit(EXIT_FAILURE);
    }

    char* line = NULL;
    size_t len = 0;
    size_t read;

    // Читаем и ищем адрес вхождением
    while ((read = getline(&line, &len, fp)) != (size_t)-1) {
        if ((strstr(line, ipv6_addr_input) != NULL)) {
            ret = 0;
        }
    }

    fclose(fp);
    if (line) free(line);

    if (ret != 0) ret = 1;

    END:
    if (fd) close(fd);
    if (ptr != MAP_FAILED && ptr != NULL) munmap(ptr, st.st_size);

    // обновление errno
    errno = saved_errno;

    return ret;
    
}

int is_valid_addr(char * s) 
{
	int arg_len = strlen(s);

            char *tmp = (char *) malloc (sizeof(char) * (arg_len+1));
            char *saved_tmp_p=tmp;
            if (!tmp) 
            {
                perror("malloc"); 
                return -1;
            }
            for(int j=0; j <= arg_len; j++) {
                tmp[j] = s[j];
            }
	    char *s_num;
            char * endptr;
            int okt_c=0;
            //2000 0db8 0000 0000 0000 0000:0000:7334
           // Логика в том чтобы подсчитать количество двух двоеточий подряд, и оставить только случаи когда мы однозначно определяем адрес
            //2001::0db8
            int num;
            int error=0;
            int space=0; // двойные двоеточия(количество)
            //char *x="1234";
            while ((s_num = strsep (&tmp,":")) != NULL) // рассматриваем октеты в цикле
            {
            	num=strtol(s_num, &endptr, 16);
            	
            	if ((s_num + strlen(s_num)) == endptr)
            	{
            	
            	
            	
            		if ((num==0)&&(strlen(s_num)==0))
            		{
            		//printf("here0\n");
            		if (space) 
            		{
            		
            		error=1;
            		}
            		 else space=1;          		
            		}
            		else{
            		 okt_c++;
            	            }
            			
            	}else 
  		 { 
                error=1;             
                 }	
                      
    	    } 
    	    
            
			
	
		if (((okt_c == 8)&&(space))||((okt_c <8)&&(space!=1))||(okt_c > 8)) // а здесь как раз накладываем условия однозначности
		{
		//printf("C=%d Space=%d\n",okt_c, space);
		//printf("here1\n");
		error=1;
		}
		//printf("C=%d Space=%d\n",okt_c, space);
		
		if(saved_tmp_p) free(saved_tmp_p);
		if (error) {
		fprintf(stderr,"%s: invalid argument '%s'\n", g_lib_name, s);
		return -1;
			   }
			   return 0;
}


