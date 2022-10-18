
#define _GNU_SOURCE         // for get_current_dir_name()
#define _XOPEN_SOURCE 500   // for nftw()
#define UNUSED(x) (void)(x)
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ftw.h>
#include <getopt.h>
#include <dlfcn.h>
#include <dirent.h>
#include <unistd.h>
#include "plugin_api.h"

int walk_func(const char*, const struct stat*, int, struct FTW*);
void atexit_func();

const char *find_dinamical_lib(const char *filename) {   // ну библиотеку найти надо для начала
    const char *so = strrchr(filename, '.');   
    if(!so || so==filename) return "";   
    return so + 1;
}						  
struct opt_logical {     
    unsigned int NOT;
    unsigned int AND;
    unsigned int OR;
};

struct informlib {
    struct plugin_option *opts;  
    char *name;	
    int opts_len;  
    void *dl;  
};

struct opt_logical logical = {0, 0, 0}; // по дефолту 0 0 0
struct informlib *all_libs;
struct option *long_optionsions;
char *path_of_plugin;  
int count_of_libs = 0;
int count_of_argumentes = 0;
char **input_argumentes = NULL;
int *long_options_index= NULL;
int *dl_index;
int isPset = 0;

int main(int argc, char *argv[]) {
    if (argc == 1) {
        fprintf(stderr, "Usage: %s <write directory>\nFor help enter flag -h\n", argv[0]);
        return 1;
    }
    char *search_dir = "";
    path_of_plugin = get_current_dir_name();
    atexit(&atexit_func);

    
    for(int i=0; i < argc; i++) { // обрабатываем -P
        if (strcmp(argv[i], "-P") == 0) {
            if (isPset) {		
                fprintf(stderr, "ERROR: the option cannot be repeated -- 'P'\n");
                return 1;
            }
            if (i == (argc - 1)) {
                fprintf(stderr, "ERROR: option 'P' needs an argument\n");
                return 1;
            }	
            DIR *d;
            if ((d=opendir(argv[i+1])) == NULL) {     
                perror(argv[i+1]);
                return 1;
            } 
            else {
            	closedir(d);
            	free(path_of_plugin);
                path_of_plugin = argv[i+1];			
                isPset = 1;
            }   
        }
        else if (strcmp(argv[i],"-v") == 0){  // Обрабатываем -v
            printf(" Лабораторная работа №1.\n Выполнил: Назаров Максим \n Вариант: 9.\n");
                return 0;
        }
        else if (strcmp(argv[i],"-h") == 0) {
            printf("-P <dir>  Задать каталог с плагинами.\n");
            printf("-A        Объединение опций плагинов с помощью операции «И» (действует по умолчанию).\n");
            printf("-O        Объединение опций плагинов с помощью операции «ИЛИ».\n");
            printf("-N        Инвертирование условия поиска (после объединения опций плагинов с помощью -A или -O).\n");
            printf("-v        Вывод версии программы и информации о программе (ФИО исполнителя, номер группы, номер варианта лабораторной).\n");
            printf("-h        Вывод справки по опциям.\n");
            return 0;
        } 
    }
   
    DIR *d;
    struct dirent *dir;
    d = opendir(path_of_plugin);
    if (d != NULL) {
        while ((dir = readdir(d)) != NULL) {
            if ((dir->d_type) == 8) {		
                count_of_libs++;   			
            }
        }
        closedir(d);
    }
    else{
        perror("opendir");
        exit(EXIT_FAILURE);
    }
    all_libs = (struct informlib*)malloc(count_of_libs*sizeof(struct informlib));
    if (all_libs==0){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    count_of_libs = 0;
    d = opendir(path_of_plugin);
    if (d != NULL) {    
        while ((dir = readdir(d)) != NULL) {
            if ((dir->d_type) == 8) {
                if (strcmp(find_dinamical_lib(dir->d_name),"so")==0){
                    char filename[258];
                    snprintf(filename, sizeof filename, "%s/%s", path_of_plugin, dir->d_name);  
										       
                    void *dl = dlopen(filename, RTLD_LAZY);
                    if (!dl) {
                        fprintf(stderr, "ERROR: dlopen() failed: %s\n", dlerror());
                        continue;
                    }
                    void *func = dlsym(dl, "plugin_get_info");
                    if (!func) {
                        fprintf(stderr, "ERROR: dlsym() failed: %s\n", dlerror());
                    }
                    struct plugin_info pi = {0};
                    typedef int (*pgi_func_t)(struct plugin_info*);     
                    pgi_func_t pgi_func = (pgi_func_t)func;		

                    int ret = pgi_func(&pi);
                    if (ret < 0) {
                        fprintf(stderr, "ERROR: plugin_get_info() failed\n");
                    }             
                    all_libs[count_of_libs].name = dir->d_name;       
                    all_libs[count_of_libs].opts = pi.sup_opts;      
                    all_libs[count_of_libs].opts_len = pi.sup_opts_len;
                    all_libs[count_of_libs].dl = dl;
                    if (getenv("LAB1DEBUG")) {
       		    	    printf("DEBUG: Found library:\n\tPlugin name: %s\n\tPlugin purpose: %s\n\tPlugin author: %s\n", dir->d_name, pi.plugin_purpose, pi.plugin_author);
                    }
                    count_of_libs++;
                }
            }
        }
        closedir(d);
    }


    
    size_t opt_count = 0;
    for(int i = 0; i < count_of_libs; i++) {
        opt_count += all_libs[i].opts_len;
    }

    long_optionsions=(struct option*)malloc(opt_count*sizeof(struct option));
    if (!long_optionsions){
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    opt_count = 0;
    for(int i = 0; i < count_of_libs; i++) {
        for(int j = 0; j < all_libs[i].opts_len; j++) {
            long_optionsions[opt_count] = all_libs[i].opts[j].opt;
            opt_count++;
        }
    }

    // проверка длинных опций на существование, т.к.(getopt не умеет)
    int flag;
    for(int i=0; i < argc; i++) {
        if (strstr(argv[i], "--")) {
            flag = 0;
            for(size_t j=0; j<opt_count; j++) {
                if (strcmp(&argv[i][2], long_optionsions[j].name) == 0) {
                    flag = 1;
                }
            }
            if (flag == 0) {
                fprintf(stderr, "ERROR: option '%s' is not found in libs\n", argv[i]);
                return 1;
            }
        }
    }


    int c;
    int is_sdir_set = 0;
    int optindex = -1;
    while((c = getopt_long(argc, argv, "-P:AON", long_optionsions, &optindex))!=-1) {
        switch(c) {// Пока парсим ключи, обрабатываем логические опции
            case 'O':
                if (!logical.OR) {
                    if (!logical.AND) {
                        logical.OR = 1;
                    } 
                    else {
                        fprintf(stderr, "ERROR: can be either 'A' or 'O' \n");
                        return 1;
                    } 
                } 
                else {
                    fprintf(stderr, "ERROR: the option 'O' can't be repeated\n");
                    return 1;
                }
                break;
            case 'A':
                if (!logical.AND) {
                    if (!logical.OR) {
                        logical.AND = 1;
                    } 
                    else {
                        fprintf(stderr, "ERROR: can be either 'A' or 'O' \n");
                        return 1;
                    }
                } 
                else {
                    fprintf(stderr, "ERROR: the option 'A' cannot be repeated\n");
                    return 1;
                }
                break;
            case 'N':
                if (!logical.NOT){
                    logical.NOT = 1;
                }
                else{
                    fprintf(stderr, "ERROR: the option cannot be repeated -- 'N'\n");
                    return 1;
                }
                break;
            case 'P':
            	break;
            case ':':
                return 1;
            case '?':
                return 1;
            default:
                //printf("here\n");
                if(optindex != -1) {
                    count_of_argumentes++;
                    if (getenv("LAB1DEBUG")) {
                        printf("DEBUG: Found option '%s' with argument: '%s'\n", long_optionsions[optindex].name, optarg);
                    }
                    long_options_index = (int*) realloc (long_options_index, count_of_argumentes * sizeof(int));
                    if (!long_options_index){
                        perror("realloc");
                        exit(EXIT_FAILURE);
                    }
                    input_argumentes = (char **) realloc (input_argumentes, count_of_argumentes * sizeof(char *));
                    if (!input_argumentes){
                        perror("realloc");
                        exit(EXIT_FAILURE);
                    }		 
                    input_argumentes[count_of_argumentes - 1] = optarg;
                    long_options_index[count_of_argumentes - 1] = optindex;
                    optindex = -1;
                } 
                else {
                    if (is_sdir_set) {
                        fprintf(stderr, "ERROR: the examine directory has already been set at '%s'\n", search_dir );
                        return 1;
                    }
                    if ((d = opendir(optarg))== NULL) {    
                        perror(optarg);
                        return 1;
                    } 
                    else {
                        search_dir  = optarg;  
                        is_sdir_set = 1;
                        closedir(d);
                    }
                }
        } 
    }

    if (strcmp(search_dir , "") == 0) {
        fprintf(stderr, "ERROR: The directory for research isn't specified\n");
        return 1;
    }
    if (getenv("LAB1DEBUG")) {
        printf("DEBUG: Directory for research: %s\n", search_dir );
    }
    if ((logical.AND==0) && (logical.OR==0) ) { 
        logical.AND = 1;		
    }
    
    if (getenv("LAB1DEBUG")) {
        printf("DEBUG: Information about input logicals:\n\tAND: %d\tOR: %d\tNOT: %d\n", logical.AND, logical.OR, logical.NOT);
    }
    dl_index = (int *)malloc(count_of_argumentes*sizeof(int));
    if (!dl_index){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    for(int i=0; i < count_of_argumentes; i++) {
        const char *opt_name = long_optionsions[long_options_index[i]].name;
        for(int j=0; j < count_of_libs; j++) {
            for(int k=0; k < all_libs[j].opts_len; k++) {
                if (strcmp(opt_name, all_libs[j].opts[k].opt.name) == 0) {
                    // ищем индекс библиотеки в массиве all_libs для текущей опции 
                    dl_index[i] = j;
                }
            }
        }
    }
	
    if (getenv("LAB1DEBUG")) {
        printf("DEBUG: Directory with libraries: %s\n", path_of_plugin);
    }
    int res = nftw(search_dir , walk_func, 20, FTW_PHYS || FTW_DEPTH);
    if (res < 0) {
        perror("nftw");
        return 1;
    }
    return 0;

}
   
typedef int (*proc_func_t)( const char *name, struct option in_opts[], size_t in_opts_len);
int walk_func(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F) {    
        int rescondition = logical.NOT ^ logical.AND;
        for (int i = 0; i < count_of_argumentes; i++) {
            struct option opt = long_optionsions[long_options_index[i]];
            char * arg = input_argumentes[i];
            if (arg) {
                opt.has_arg = 1;
                opt.flag = (void *)arg;
            } else {
                opt.has_arg = 0;
            }
    	    void *func = dlsym(all_libs[dl_index[i]].dl, "plugin_process_file"); // Загружаем динамическую библиотеку
            proc_func_t proc_func = (proc_func_t)func;
            int res_func;
            res_func = proc_func(fpath, &opt, 1);
            if (res_func) {
                if (res_func > 0) {
                    res_func = 0;
                } 
                else {
                    fprintf(stderr, "Plugin execution error\n");
                    return 1;
                }
            } 
            else {
                res_func = 1;
            }
                 
            if (logical.NOT ^ logical.AND) {
                if (!(rescondition = rescondition & (logical.NOT ^ res_func)))
                    break;
            } else {
                if ((rescondition = rescondition | (logical.NOT ^ res_func)))
                    break;
            }
        }

        if (rescondition) {
            printf("%s\n",fpath);
        }
        else{
            if (getenv("LAB1DEBUG")) {
                printf("\tDEBUG: File: %s doesn't match the search criteria\n", fpath);
            }
        }
    }	
    		
    UNUSED(sb); 
    UNUSED(ftwbuf);
    return 0;
}


void atexit_func() {
    for (int i=0;i<count_of_libs;i++){
        if (all_libs[i].dl) dlclose(all_libs[i].dl);
    }// Высвобождаем память
    if (all_libs) free(all_libs);
    if (dl_index) free(dl_index);
    if (long_optionsions) free(long_optionsions);
    if (long_options_index) free(long_options_index);
    if (input_argumentes) free(input_argumentes);
    if (!isPset) free(path_of_plugin);
}



