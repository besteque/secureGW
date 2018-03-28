/*
 * common.c
 *
 *  Created on: 2018-3-21
 *      Author: xuyang
 */


#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>

#include "pub.h"
#include "common.h"





static void pabort(const uint8_t *s)
{
    perror(s);
    abort();
    //strerror(errno)
}


/**
 * time farmat, e.g. 1970-01-01 08:07:58.951
 */
void getcurtime(uint8_t *dtime, uint32_t len)
{
    time_t now ;
    struct tm *tm_now;
    struct timeval tm_val;
    uint32_t data_len;
    uint8_t buffer[DATE_TIME_STR_LEN_MAX] = {0};

    time(&now) ;

    tm_now = localtime(&now) ; 
    
    data_len = snprintf(buffer, DATE_TIME_STR_LEN_MAX, "%d.%d.%d %d:%d:%d.", 
                            tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday, 
                            tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);

    gettimeofday(&tm_val, NULL);
    data_len += snprintf(buffer+data_len, DATE_TIME_STR_LEN_MAX-data_len, "%ld", tm_val.tv_usec/1000);
    

    if (data_len >= len)
    {
        pabort("getcurtime failed.\n");
        return;
    }

    strncpy(dtime, buffer, len);
}





/**
 * user msg length should be less than 988 bytes, or may lead stack overflow!
 * e.g. [1970-01-01 09:17:00.887][IPC]receive msg,frame_id:0x4006
 * return:string length
 */
uint32_t print_sys_msg(const uint8_t *module, const uint8_t *fmt, ...)
{
    int n;
    va_list args;
    uint8_t msg[PRINT_MSG_LEN_MAX] = {0};
    uint8_t now_tm[DATE_TIME_STR_LEN_MAX] = {0};

    if ((strlen(fmt) == 1) && (fmt[0] == '\n'))
    {
        printf("\n");
        return 1;
    }

    getcurtime(now_tm, DATE_TIME_STR_LEN_MAX);

    va_start(args, fmt);
    n  = sprintf(msg, "[%s][%s]", now_tm, module);
    n += vsnprintf(&msg[n], PRINT_MSG_LEN_MAX, fmt, args);
    va_end(args);

    printf("%s\n", msg);

    return n;
}

uint32_t rel_slogf(const uint8_t *fmt, ...)
{
    int         status;
    va_list     arg;

    va_start(arg, fmt);
    status = vfprintf(stderr, fmt, arg);
    status += fprintf(stderr, "\n");
    va_end(arg);
    return status;
}


#if 0


/**
 * get pid by proc name, return -1 if not exist
 */
uint32_t getpid_by_name(const uint8_t* procname)
{
    FILE      *fp;
    uint32_t pid = -1;
    uint8_t      buf[FILE_PATH_NAME_LEN_MAX] = {0};
    uint8_t      cmd[FILE_PATH_NAME_LEN_MAX] = {0};

    if (!procname || (*procname == '\0'))
        return -1;

    snprintf(cmd, FILE_PATH_NAME_LEN_MAX, "ps -F\"%%a\t%%N\" | grep %s", procname);

    fp = popen( cmd, "r" );

    if (fp == NULL)
        return -1;

    if (fread( buf, sizeof(uint8_t), FILE_PATH_NAME_LEN_MAX, fp ) > 0)
        pid = strtoul(buf, 0, 0);

    pclose(fp);

    return pid;
}

#endif


void dbg_print_cur_dir(void)
{
    int8_t buf[FILE_PATH_NAME_LEN_MAX];   
    getcwd(buf,sizeof(buf));   
    printf("current working directory: %s\n", buf); 

}


/*void record_dev_id(int8_t *id)
{
    uint32_t  len = strlen(id);

    if (len >= DEV_ID_LEN_MAX)
        return;
    
    strncpy(session_id, id, len);
    session_id[len] = '\0';
    
    printf("record session_id: %s\n", session_id); 
}*/


void get_dev_id(int8_t *id)
{
    uint32_t index;
    proc_spec_data_t *priv;

    //strncpy(id, session_id, strlen(session_id));

    get_proc_priv_data(&priv);
    index = get_task_serialno();
    
    strncpy(id, priv->task_var[index]->devid, strlen(priv->task_var[index]->devid));
}


/*
 *   WARNing: one star means passed value,only!
 *   get process private data
 */
uint32_t get_proc_priv_data(proc_spec_data_t **priv)
{
    if (!proc_data)
    {
        log_info(MSG_LOG_DBG, DBG, "proc_data is null");
        return ERROR;
    }
    
    *priv = proc_data;

    return OK;
}


//////////////////////////////////////////////////////
// stub 
// in thread context, it can get from task var. todo!
uint32_t get_task_serialno(void)
{
    return 0;
}


void dbg_print_msg_head(msg_head_t *head)
{
    log_info(MSG_LOG_DBG, DBG, "msg head(size:%ld) as follow:", sizeof(msg_head_t));
    log_info(MSG_LOG_DBG, DBG, "----------------------------------");
    log_info(MSG_LOG_DBG, DBG, "\t magic        :%s", head->magic);
    log_info(MSG_LOG_DBG, DBG, "\t type         :%d", head->type);
    log_info(MSG_LOG_DBG, DBG, "\t date_len     :%d", head->data_len);
    log_info(MSG_LOG_DBG, DBG, "\t version      :%d", head->version);
    log_info(MSG_LOG_DBG, DBG, "\t trans_id     :%d", head->trans_id);
    log_info(MSG_LOG_DBG, DBG, "\t total_length :%ld", head->total_length);
    log_info(MSG_LOG_DBG, DBG, "\t total_package:%d", head->total_package);
    log_info(MSG_LOG_DBG, DBG, "\t index        :%d", head->index);
    log_info(MSG_LOG_DBG, DBG, "----------------------------------");
    log_info(MSG_LOG_DBG, DBG, "\n");
}

void dbg_print_devinfo(dev_info_t    *devinfo)
{
    log_info(MSG_LOG_DBG, DBG, "\t id        :%s", devinfo->id);
    log_info(MSG_LOG_DBG, DBG, "\t dev_type  :%ld", devinfo->dev_type);
    //log_info(MSG_LOG_DBG, DBG, "\t algorithm :%ld", devinfo->algorithm);
    //log_info(MSG_LOG_DBG, DBG, "\n");

    // to be continued...  sign_data/crypt_type
}


void dbg_print_dev_list(struct list_head *head)
{
    dev_info_t   *pos, *n;
    dev_info_t   *devinfo;

    if (list_empty(head))
    {
        log_info(MSG_LOG_DBG, DBG, "list is empty");
        return;
    }
    
    log_info(MSG_LOG_DBG, DBG, "devinfo(size:%ld) as follow:", sizeof(dev_info_t));
    log_info(MSG_LOG_DBG, DBG, "-----------------------------------");

#if 1   // both OK
    list_for_each_entry_safe(pos, n, head, point)
    {
        devinfo = list_entry(&pos->point, dev_info_t, point);
        dbg_print_devinfo(devinfo);
    }
    log_info(MSG_LOG_DBG, DBG, "-----------------------------------");
    log_info(MSG_LOG_DBG, DBG, "\n");

#else
    struct list_head *pt, *nt;

     list_for_each_safe(pt, nt, head)
    {
        devinfo = list_entry(pt, dev_info_t, point);
        dbg_print_devinfo(devinfo);
    }
#endif

}


void dbg_print_char_in_buf(int8_t *buf, uint32_t len)
{
    uint32_t i;
    log_info(MSG_LOG_DBG, DBG, "buf info as follow:");

    for (i = 0; i < len; i++)
    {
        if (/*(buf[i] == '\n') || (buf[i] == '\t')||*/ (buf[i] == '\0'))
            return;
        printf( "%c", buf[i]);
        
        if ((i+1)%256 == 0)
            printf("\n");
    }
    
    printf("\n");        
}

