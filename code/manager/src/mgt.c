/*
 * mgt.c
 *
 *  Created on: 2018-3-22
 *      Author: xuyang
 */
    
#include "crypt.h"
#include "mgt.h"






int int_device_list(struct list_head *head)
{
    INIT_LIST_HEAD(head);
}

int list_add_device(dev_info_t *info, struct list_head *head)
{
    proc_spec_data_t *priv;
    struct list_head *pos;
    dev_info_t      *dev;
    dev_info_t      *node;
    uint32_t index;

    
    if (list_empty(head))
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "devlist empty, create it");
        goto ADD_LIST;
    }

     list_for_each(pos, head)
    {
        dev = list_entry(pos, dev_info_t, point);

        if (!strncmp(dev->id, info->id, strlen(info->id)))
        {
            return OK;
        }
    }

ADD_LIST:
    get_proc_priv_data(&priv);
    index = get_task_serialno();    
    strncpy(priv->task_var[index]->devid, info->id, strlen(info->id));

    /*****************************************************************************
    * WARNing: new node must apply heap memeroy! stack will recycle after return
    ******************************************************************************/
    node = (dev_info_t*)malloc(sizeof(dev_info_t));
    if (node == NULL)
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "malloc dev_info_t failed");
        return ERROR;
    }
    memcpy(node, info, sizeof(dev_info_t));
    
    list_add(&node->point, head);
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "add node OK");
    
    return OK;
}


uint32_t validate_data(int8_t *msg, uint32_t len)
{
    msg_head_t head;
    

    if (len < sizeof(head))
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "len(%d) < sizeof(msg_head)(%ld)", len, sizeof(head));
        return ERROR;
    }

    memcpy(&head, msg, sizeof(head));

    if (strncmp(head.magic, MAGIC_WORD, strlen(MAGIC_WORD)))
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "msg_head magic incorrect: %s", head.magic);
        return ERROR;
    }

    if ((head.type > MAX_MSG_TYPE) || (head.type <= MSG_TYPE_INIT))
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "msg_head type %d unrecognized", head.type);
        return ERROR;
    }

    return OK;
}


/*
    preserve TE device info in a list
*/
uint32_t handle_login_req(int8_t *msg, uint32_t len)
{
    msg_head_t          head;
    login_data_t        log_data;
    dev_info_t          devinfo = {0};
    proc_spec_data_t *priv;

    // multi-pkg todo...

    if (len != (sizeof(head) + sizeof(log_data)))
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "len != (sizeof(msg_head) + sizeof(log_data))");
        return ERROR;
    }

    memcpy(&head, msg, sizeof(head));
    memcpy(&log_data, msg+sizeof(head), sizeof(log_data));

    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "rcv msgid:%#x, devid:%s", head.type, log_data.dev_id);
    
    get_proc_priv_data(&priv);

    strncpy(devinfo.id, log_data.dev_id, strlen(log_data.dev_id));
    list_add_device(&devinfo, &priv->dev_list_head);
    
    //strncpy(devnode.id, log_data.dev_id, strlen(log_data.dev_id));
    

    // stub
    dbg_print_dev_list(&priv->dev_list_head);

    return OK;
}



uint32_t handle_signiture_req(int8_t *msg, uint32_t len)
{
    int32_t ret, index;
    msg_head_t          head;
    signiture_data_t         sign_data;
    dev_info_t          devinfo = {0};
    int8_t devid[DEV_ID_LEN_MAX] = {0};
    proc_spec_data_t *priv;

    // multi-pkg todo...

    if (len != (sizeof(head) + sizeof(sign_data)))
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "handle_signiture_req:len != (sizeof(msg_head) + sizeof(sign_data))");
        return ERROR;
    }

    memcpy(&head, msg, sizeof(head));
    memcpy(&sign_data, msg+sizeof(head), sizeof(sign_data));

    // stub
    //dbg_print_dev_list(&dev_list_head);

    get_dev_id(devid);
    get_devinfo_by_devid(devid, &devinfo);

    // need decrypt ????
    //ret = IW_SM2_DecryptData(cipher, strlen(cipher), pdata, &pdataLen);

    // save data
    memcpy(&devinfo.sign_data, &sign_data, sizeof(signiture_data_t));
    update_devinfo_by_devid(devid, &devinfo);
    
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "client %s sign data:%s", devid, sign_data.data);

    // server verify client data
    get_proc_priv_data(&priv);
    index = get_task_serialno();
    
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "priv->task_var[index].devid:%s", priv->task_var[index]->devid);
    
    ret = IW_VerifyData(priv->pub_matrix, PUB_KEY_MATRIX_LEN_MAX, 
                    CLIENT_VERIFY_DATA_SYMBOL, strlen(CLIENT_VERIFY_DATA_SYMBOL), 
                    sign_data.data, priv->task_var[index]->devid);
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "IW_VerifyData ret:%d", ret);

    return OK;
}


/* key enveloped by DE */
uint32_t negotiate_crypt_type(int8_t *msg, uint32_t len)
{
    int32_t ret;
    msg_head_t          head;
    encrypt_data_t         crypt_data;
    dev_info_t          devinfo = {0};
    int8_t devid[DEV_ID_LEN_MAX] = {0};
    int8_t symmetric_key[CIPHER_DATA_LEN_MAX] = {0};
    int32_t sym_key_len = 0;

    if (len != (sizeof(head) + sizeof(encrypt_data_t)))
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "negotiate_crypt_type:len(%ld) !=%ld (sizeof(msg_head) + sizeof(encrypt_data_t)",
                                    len, sizeof(head) + sizeof(encrypt_data_t));
        return ERROR;
    }

    // multi-pkg todo...
    memcpy(&head, msg, sizeof(head));
    memcpy(&crypt_data, msg+sizeof(head), sizeof(crypt_data));

    get_dev_id(devid);
    get_devinfo_by_devid(devid, &devinfo);

    // need decrypt ????
    //ret = IW_SM2_DecryptData(cipher, strlen(cipher), pdata, &pdataLen);

    // save data
    ret = IW_SM2_OpenEnv(crypt_data.key, symmetric_key, &sym_key_len);
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "IW_SM2_OpenEnv ret:%d", ret);
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "negotiate_crypt_type crypt_data.key:%s", crypt_data.key);
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "negotiate_crypt_type key after:%s", symmetric_key);
    
    memcpy(&devinfo.crypt_type, &crypt_data, sizeof(encrypt_data_t));
    memset(devinfo.crypt_type.key, 0, SECRET_KEY_LEN_MAX); // caution len different!
    memcpy(&devinfo.crypt_type.key, symmetric_key, sym_key_len);
    update_devinfo_by_devid(devid, &devinfo);
    
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "dev %s affirmed crypt type:%#x", devid, crypt_data.algorithm);

    return OK;
}


// len:plain data len -> modify to recv data len
uint32_t decrypt_usr_data(int8_t *data, uint32_t len)
{
    int32_t ret;
    uint8_t *plain_date;
    int32_t plain_len = 0;
    proc_spec_data_t *priv;
    int8_t devid[DEV_ID_LEN_MAX] = {0};
    dev_info_t devinfo = {0};

    plain_date = (uint8_t*)malloc(PACKAGE_DATA_LEN_MAX);
    if ( plain_date == NULL)
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "malloc plain_date failed");
        return ERROR;
    }
    memset(plain_date, 0, sizeof(PACKAGE_DATA_LEN_MAX));
    
    get_dev_id(devid);
    get_devinfo_by_devid(devid, &devinfo);
    
    //ret = IW_SM4_DECRYPT(SM4_MODE_ECB, SM4_NOPADDING, NULL, devinfo.crypt_type.key, data,
    //                   len, plain_date, &plain_len);
    
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "strlen(data):%d, len:%d", strlen(data), len);
    
    ret = decrypt_data(&devinfo.crypt_type, data, len, plain_date, &plain_len);
    if (ret != OK)
    {        
        //PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "IW_SM2_DecryptData failed, ret:%d", ret);
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "decrypt_data failed, ret:%d", ret);
        free((char*)plain_date);
        return ERROR;
    }

    //stub
    if (plain_len > 0)
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "decrypt_usr_data as follow:");
        dbg_print_char_in_buf(plain_date, plain_len);
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "plain_date:%s\n", plain_date);
    }

    if (plain_date)
    {
        free((char*)plain_date);
        plain_date = NULL;
    }

    return OK;
}

// len: tcp pkg size, include head
uint32_t rcv_usr_data(int8_t *msg, uint32_t len)
{
    int32_t ret;
    msg_head_t          head;
    dev_info_t          devinfo = {0};
    int8_t devid[DEV_ID_LEN_MAX] = {0};
    int8_t *usr_data;

    // multi-pkg todo...

    memcpy(&head, msg, sizeof(head));

    get_dev_id(devid);
    get_devinfo_by_devid(devid, &devinfo);

    // need decrypt ????
    //ret = IW_SM2_DecryptData(cipher, strlen(cipher), pdata, &pdataLen);

    usr_data = (int8_t*)malloc(PACKAGE_DATA_LEN_MAX);
    if (usr_data == NULL)
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "malloc(PACKAGE_DATA_LEN_MAX) failed");
        return ERROR;
    }
    
    bzero(usr_data, PACKAGE_DATA_LEN_MAX);
    strcpy(usr_data, msg+sizeof(msg_head_t));
    ret = decrypt_usr_data(usr_data, head.data_len);
    if (ret != OK)
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "rcv dev %s usr data", devid);
        return ERROR;
    }

    // usr data
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "rcv dev %s usr data", devid);
    dbg_print_msg_head(&head);

    if (usr_data)
    {
        free((char*)usr_data);
        usr_data = NULL;
    }

    return OK;
}


/**
*   deal TE request data
*/
uint32_t parse_data(int8_t *msg, uint32_t len)
{
    int32_t ret = OK, index;
    msg_head_t head;
    proc_spec_data_t *priv;

    memcpy(&head, msg, sizeof(head));
    //PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "svr rcv msgid %#x", head.type);
    dbg_print_msg_head(&head);

    switch (head.type)
    {
        case MSG_TYPE_LOGIN:
            ret = handle_login_req(msg, len);
            break;

        
        case MSG_TYPE_SIGNITURE:
            ret = handle_signiture_req(msg, len);
            break;
            
        case MSG_TYPE_ENCRYPT_INFO:
            ret = negotiate_crypt_type(msg, len);
            break;
            
        case MSG_TYPE_USR_DATA:
            ret = rcv_usr_data(msg, len);
            break;

        default:break;
    }


    if (ret != OK)
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "parse_data failed, ret:%d", ret);
        return FINISH;
    }

    // need adapt, use original data_len
    get_proc_priv_data(&priv);
    index = get_task_serialno();
    calc_total_len(priv->task_var[index], head.data_len);
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "receive data total len:%ld, expect:%ld", 
                         get_total_len(), head.total_length);
        
    if (get_total_len() >= head.total_length)
    {
        return FINISH;
    }

    return ret;
}



uint32_t handle_login_ack(int8_t **data, uint32_t *len)
{
    int8_t      *buf;
    uint32_t    data_len;
    msg_head_t *head;
    login_data_t *login_data;
    proc_spec_data_t *proc;

    data_len = sizeof(msg_head_t) + sizeof(login_data_t);

    buf = (int8_t  *)malloc(data_len);
    if (buf == NULL)
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "malloc buf failed");
        return ERROR;
    }

    memset(buf, 0, data_len);

    *len = data_len;
    *data = buf;

    head = (msg_head_t*)buf;
    head->type = MSG_TYPE_LOGIN;
    head->data_len = sizeof(login_data_t);
    head->total_length = head->data_len;
    head->total_package = 1;
    strncpy(head->magic, MAGIC_WORD, MAGIC_WORD_LEN_MAX);

    get_proc_priv_data(&proc);

    login_data = (login_data_t*)((msg_head_t*)buf +1);
    memcpy(login_data->dev_id, proc->devid, strlen(proc->devid));
    
    return OK;
}


uint32_t handle_sign_ack(int8_t **data, uint32_t *len)
{
    int32_t ret;
    int8_t      *buf;
    uint32_t    data_len;
    msg_head_t *head;
    signiture_data_t *sign_data;
    dev_info_t          devinfo = {0};
    int8_t devid[DEV_ID_LEN_MAX] = {0};
    int8_t sign_val[SIGN_DATA_LEN_MAX] = {0};
    int8_t sign_final[SIGN_DATA_LEN_MAX] = {0};
    proc_spec_data_t *priv;

    data_len = sizeof(msg_head_t) + sizeof(signiture_data_t);

    buf = (int8_t  *)malloc(data_len);
    if (buf == NULL)
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "malloc buf failed");
        return ERROR;
    }

    memset(buf, 0, data_len);

    *len = data_len;
    *data = buf;

    head = (msg_head_t*)buf;
    head->type = MSG_TYPE_SIGNITURE;
    head->data_len = sizeof(signiture_data_t);
    head->total_length = head->data_len;
    head->total_package = 1;
    strncpy(head->magic, MAGIC_WORD, MAGIC_WORD_LEN_MAX);

    get_dev_id(devid);
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "handle_sign_ack devid:%s", devid);
    get_devinfo_by_devid(devid, &devinfo);
    
    //dbg_print_char_in_buf(devinfo.sign_data.data, SIGN_DATA_LEN_MAX);
    //PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "original data:%s", devinfo.sign_data.data);

    // encrypt according Base64
    /*ret = IW_ServerSignData(devinfo.sign_data.data, sign_val);
    if (ret != OK)
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "IW_ServerSignData failed, code %d");
        return ERROR;
    }
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "ret code %d, after sign:%s", ret, sign_val);
    */

    dbg_print_devinfo(&devinfo);
    
    //PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "sign data:%s", sign_val);

    sign_data = (signiture_data_t*)((msg_head_t*)buf +1);

    // need encrypt whole sign data 'sign_val', then asign to 'sign_data' ?
    //get_proc_priv_data(priv);
    //strncpy(sign_val, SERVER_VERIFY_DATA_SYMBOL, strlen(SERVER_VERIFY_DATA_SYMBOL));
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "[test]ack to clent:");
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "[test]original data:%s", SERVER_VERIFY_DATA_SYMBOL);
    ret = IW_SignData(SERVER_VERIFY_DATA_SYMBOL, strlen(SERVER_VERIFY_DATA_SYMBOL), sign_val);
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "[test]sign_val data:%s", sign_val);
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "IW_SignData ret:%d", ret);
    ret = IW_ServerSignData(sign_val, sign_final);
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "[test]sign_final data:%s", sign_final);
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "IW_ServerSignData ret:%d", ret);
    memcpy(sign_data->data, sign_final, strlen(sign_final));

    return OK;
    
}


uint32_t affirm_crypt_type(int8_t **data, uint32_t *len)
{
    int32_t ret;
    int8_t      *buf;
    uint32_t    data_len;
    msg_head_t *head;
    encrypt_data_t *crypt_data;
    dev_info_t          devinfo = {0};
    int8_t devid[DEV_ID_LEN_MAX] = {0};
    proc_spec_data_t *priv;
    int8_t evn[CIPHER_DATA_LEN_MAX] = {0};

    data_len = sizeof(msg_head_t) + sizeof(encrypt_data_t);

    buf = (int8_t*)malloc(data_len);
    if (buf == NULL)
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "affirm_crypt_type malloc failed");
        return ERROR;
    }

    memset(buf, 0, data_len);

    *len = data_len;
    *data = buf;

    head = (msg_head_t*)buf;
    head->type = MSG_TYPE_ENCRYPT_INFO;
    head->data_len = sizeof(encrypt_data_t);
    head->total_length = head->data_len;
    head->total_package = 1;
    strncpy(head->magic, MAGIC_WORD, MAGIC_WORD_LEN_MAX);

    get_dev_id(devid);
    get_devinfo_by_devid(devid, &devinfo);

    crypt_data = (encrypt_data_t*)((msg_head_t*)buf +1);
    
    // need encrypt whole data 'devinfo.crypt_type', then asign to 'crypt_data' ?


    get_proc_priv_data(&priv);

    //ret = IW_SM2_MakeEnv(priv->pub_matrix, PUB_KEY_MATRIX_LEN_MAX, devid,
    //                            devinfo.crypt_type.key, strlen(devinfo.crypt_type.key), evn);
    
    //PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "IW_SM2_MakeEnv ret:%d", ret);

    
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "affirm_crypt_type crypt_data->key:%s", crypt_data->key);
    
    memcpy(crypt_data, &devinfo.crypt_type, sizeof(encrypt_data_t));
    //memset(devinfo.crypt_type.key, 0, SECRET_KEY_LEN_MAX); // caution len different!
    //memcpy(crypt_data->key, evn, strlen(evn));

    return OK;
    
}


// common date transmit API
uint32_t response_to_client(int8_t **data, uint32_t *len)
{
    int32_t     ret;
    int8_t      *buf;
    uint32_t    data_len;
    msg_head_t *head;
    dev_info_t devinfo = {0};
    int8_t devid[DEV_ID_LEN_MAX] = {0};
    proc_spec_data_t *priv;
    int8_t pub_key[PUB_KEY_LEN_MAX] = {0};
    int8_t cipher[CIPHER_DATA_LEN_MAX] = {0};
    uint32_t cipher_len = 0;

    data_len = sizeof(msg_head_t) + CIPHER_DATA_LEN_MAX;
    
    buf = (int8_t*)malloc(data_len);
    if (buf == NULL)
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "response_to_client malloc failed");
        return ERROR;
    }

    memset(buf, 0, data_len);

    *data = buf;

    head = (msg_head_t*)buf;
    head->type = MSG_TYPE_USR_DATA;
    head->total_package = 1;
    strncpy(head->magic, MAGIC_WORD, MAGIC_WORD_LEN_MAX);

    get_dev_id(devid);
    get_devinfo_by_devid(devid, &devinfo);
    
    // need encrypt whole data 'devinfo.crypt_type', then asign to 'crypt_data' ?
    
    get_proc_priv_data(&priv);
    ret = CPK_Get_IPK(devid, priv->pub_matrix, PUB_KEY_MATRIX_LEN_MAX, pub_key);
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "CPK_Get_IPK ret:%d", ret);
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "devid:%s", devid);
    
    ret = IW_SM2_EncryptData(pub_key, LITERIAL_TEXT_FOR_TEST, strlen(LITERIAL_TEXT_FOR_TEST)+1, cipher);
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "IW_SM2_EncryptData ret:%d", ret);
    cipher_len = strlen(cipher);
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "response_to_client cipher_len:%d", cipher_len);
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "response_to_client cipher:%s", cipher);
    
    /*ret = IW_SM4_ENCRYPT(SM4_MODE_ECB, SM4_NOPADDING, NULL, devinfo.crypt_type.key, 
                    LITERIAL_TEXT_FOR_TEST, ((strlen(LITERIAL_TEXT_FOR_TEST)+SYMMETRIC_KEY_LEN-1)/SYMMETRIC_KEY_LEN)*SYMMETRIC_KEY_LEN, cipher, &cipher_len);
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "IW_SM4_ENCRYPT ret:%d", ret);
    PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "cipher_len:%d", cipher_len);*/
    
    memcpy((char*)((msg_head_t*)buf +1), cipher, cipher_len);    
    *len = sizeof(msg_head_t) + cipher_len;
    
    head->data_len = cipher_len;
    head->total_length = head->data_len;

    return OK;
}


uint32_t send_err_ack(uint32_t fd, int8_t **data)
{
    int32_t ret;
    uint32_t    data_len;
    int8_t      *buf; 
    msg_head_t *head;

    data_len = sizeof(msg_head_t);
    
    buf = (int8_t*)malloc(data_len);
    if (buf == NULL)
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, MGT, "send_err_ack malloc failed");
        return ERROR;
    }

    *data = buf;

    head = (msg_head_t*)buf;
    head->type = MSG_TYPE_USR_DATA;
    head->data_len = 0;
    head->total_length = head->data_len;
    head->total_package = 1;
    strncpy(head->magic, MAGIC_WORD, MAGIC_WORD_LEN_MAX);
    head->err_type = 12345;
    
    send_to_client(fd, buf, data_len);

    return OK;
}

/**
 * response data to TE
*/

uint32_t prepare_interactive_data(uint32_t msg_type, int8_t **data, uint32_t *len)
{
    int32_t ret = ERROR;

    switch (msg_type)
    {
        case MSG_TYPE_LOGIN:
            ret = handle_login_ack(data, len);
            break;

        
        case MSG_TYPE_SIGNITURE:
            ret = handle_sign_ack(data, len);
            break;
            
        case MSG_TYPE_ENCRYPT_INFO:
            ret = affirm_crypt_type(data, len);
            break;
            
        case MSG_TYPE_USR_DATA:
            ret = response_to_client(data, len);
            break;
    }

    if (ret != OK)
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, INIT, "prepare_interactive_data failed, ret:%d", ret);
        return ret;
    }

    return OK;
}



/* 
    get comm key by device id  
*/
uint32_t get_key_by_devid(int8_t *dev_id, int8_t *pk)
{
    proc_spec_data_t * priv = NULL;
    struct list_head *pos, *n;
    dev_info_t      *dev;

    get_proc_priv_data(&priv);

    if (list_empty(&priv->dev_list_head))
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, INIT, "list dev_list_head is empty");
        return ERROR;
    }

     list_for_each_safe(pos, n, &priv->dev_list_head)
    {
        dev = list_entry(pos, dev_info_t, point);

        if (!strncmp(dev->id, dev_id, strlen(dev_id)))
        {
            strncpy(pk, dev->crypt_type.key, strlen(dev->crypt_type.key));
            return OK;
        }
    }

    return ERROR;
}



/* 
    get devinfo by device id
    INT1:dev_id
    OUT1:info
*/
uint32_t get_devinfo_by_devid(int8_t *dev_id, dev_info_t *info)
{
    proc_spec_data_t * priv = NULL;
    struct list_head *pos;
    dev_info_t      *dev;

    get_proc_priv_data(&priv);

    if (list_empty(&priv->dev_list_head))
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, INIT, "list dev_list_head is empty");
        return ERROR;
    }

     list_for_each(pos, &priv->dev_list_head)
    {
        dev = list_entry(pos, dev_info_t, point);

        if (dev == NULL)
        {
            PRINT_SYS_MSG(MSG_LOG_DBG, INIT, "error, list is null");
            return ERROR;
        }

        if (!strncmp(dev->id, dev_id, strlen(dev_id)))
        {
            memcpy(info, dev, sizeof(*dev));
            return OK;
        }
    }
    
    PRINT_SYS_MSG(MSG_LOG_DBG, INIT, "error, get_devinfo_by_devid failed");

    return ERROR;
}


/*
    update devnode
    IN1:dev_id
    IN2:info
*/
uint32_t update_devinfo_by_devid(int8_t *dev_id, dev_info_t *info)
{
    proc_spec_data_t * priv = NULL;
    struct list_head *pos;
    dev_info_t      *dev;

    get_proc_priv_data(&priv);

    if (list_empty(&priv->dev_list_head))
    {
        PRINT_SYS_MSG(MSG_LOG_DBG, INIT, "list dev_list_head is empty");
        return ERROR;
    }

     list_for_each(pos, &priv->dev_list_head)
    {
        dev = list_entry(pos, dev_info_t, point);

        if (dev == NULL)
        {
            PRINT_SYS_MSG(MSG_LOG_DBG, INIT, "error, list is null");
            return ERROR;
        }

        if (!strncmp(dev->id, dev_id, strlen(dev_id)))
        {
            memcpy(dev, info, sizeof(dev_info_t));
            return OK;
        }
    }
    
    PRINT_SYS_MSG(MSG_LOG_DBG, INIT, "error, update_devinfo_by_devid failed");

    return ERROR;
}


