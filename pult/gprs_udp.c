#include "gprs_udp.h"

#include "ql_stdlib.h"
#include "ql_gprs.h"
#include "ql_socket.h"
#include "ril_network.h"
#include "ril.h"

#include "db/db.h"

#include "configurator/configurator.h"

#include "common/debug_macro.h"
#include "common/errors.h"

#include "core/system.h"
#include "core/simcard.h"
#include "core/timers.h"

#include "service/humanize.h"
#include "service/helper.h"

#include "codecs/codecs.h"


#define PDP_CONTXT_ID  1


/*****************************************************************
* define process state
******************************************************************/
typedef enum {
    STATE_NW_QUERY_STATE        = 0,
    STATE_GPRS_ACTIVATING_BEGIN = STATE_NW_QUERY_STATE,

    STATE_GPRS_CONFIG           = 1,
    STATE_GPRS_ACTIVATE         = 2,
    STATE_GPRS_ACTIVATING       = 3,
    STATE_GPRS_GET_DNSADDRESS   = 4,
    STATE_GPRS_GET_LOCALIP      = 5,
    STATE_DEST_ADDR_CHECKING    = 6,
    STATE_SOC_CREATE            = 7,
    STATE_SOC_READY_TO_USE      = 8,

    STATE_GPRS_ACTIVATING_END   = STATE_SOC_READY_TO_USE,

    STATE_GPRS_DEACTIVATE               = 20,
    STATE_GPRS_DEACTIVATING_BY_FAIL     = 21,
    STATE_GPRS_DEACTIVATING_MANUALLY    = 22
}Enum_UDP_STATE;

static u8 m_udp_state = STATE_NW_QUERY_STATE;
static Enum_GPRS_CHANNEL_Error m_last_error = GPRS_CHANNEL_WOULDBLOCK;

static u16 m_activate_iteration_counter = 0;

static u8 iStopTimerPultSession = 0;

/*****************************************************************
* Common param
******************************************************************/
static s32 m_socketid = SOCKET_INIT_VALUE;

static GprsPeer m_peer;
static s32 m_lenToUpdSend = 0;   // length of last sent datagram

/*****************************************************************
* Function prototypes
******************************************************************/
static void closeSocket(s32 socketId);
static void reactivateGprs(void);
static void setGprsLastError(Enum_GPRS_CHANNEL_Error error);
static s32  getUdpDestination(u8 *ip[], u16 *port);
static const char *gprsUdpStateByCode(u8 code);


/*****************************************************************
* GPRS and socket callback function
******************************************************************/
static void callback_socket_connect(s32 socketId, s32 errCode, void* customParam );
static void callback_socket_close(s32 socketId, s32 errCode, void* customParam );
static void callback_socket_accept(s32 listenSocketId, s32 errCode, void* customParam );
static void callback_socket_read(s32 socketId, s32 errCode, void* customParam );
static void callback_socket_write(s32 socketId, s32 errCode, void* customParam );

static void Callback_GPRS_Actived(u8 contexId, s32 errCode, void* customParam);
static void CallBack_GPRS_Deactived(u8 contextId, s32 errCode, void* customParam );
//static void Callback_GetIpByName(u8 contexId, u8 requestId, s32 errCode,  u32 ipAddrCnt, u32* ipAddr);


ST_PDPContxt_Callback  callback_gprs_func =
{
    Callback_GPRS_Actived,
    CallBack_GPRS_Deactived
};
ST_SOC_Callback        callback_soc_func=
{
    callback_socket_connect,
    callback_socket_close,
    callback_socket_accept,
    callback_socket_read,
    callback_socket_write
};


static s32 _GprsActivation_Step(void)
{
    /* check gprs activation timeout */
    if (++m_activate_iteration_counter > GPRS_ACTIVATE_ITERATION_THRESHOLD &&
            m_udp_state < STATE_GPRS_ACTIVATING_END)
    {
        OUT_DEBUG_1("GPRS activation timeout. Deactivate GPRS...\r\n")
        m_udp_state = STATE_GPRS_DEACTIVATE;
    }


    OUT_DEBUG_2("_GprsActivation_Step(): m_udp_state = %s\r\n", gprsUdpStateByCode(m_udp_state));
    OUT_DEBUG_2("m_activate_iteration_counter = %d\r\n", m_activate_iteration_counter);

    s32 ret = 0;

    switch (m_udp_state)
    {
        case STATE_NW_QUERY_STATE:
        {
            s32 cgreg = 0;
            ret = RIL_NW_GetGPRSState(&cgreg);
            OUT_DEBUG_8("RIL_NW_GetGPRSState(): cgreg=%d\r\n", cgreg);
\
            switch (cgreg) {
            case NW_STAT_REGISTERED:
            case NW_STAT_NOT_ACTIVE:
                m_udp_state = STATE_GPRS_CONFIG;
                break;
            case NW_STAT_REG_DENIED:
            case NW_STAT_UNKNOWN:
                m_udp_state = STATE_GPRS_DEACTIVATE;
            default:
                break;
            }

            break;
        }
        case STATE_GPRS_CONFIG:
        {
            SimSettings *pSettings = Ar_SIM_currentSettings();
            ST_GprsConfig  gprsCfg;

            Ql_strcpy((char *)gprsCfg.apnName, pSettings->apn);
            Ql_strcpy((char *)gprsCfg.apnUserId, pSettings->login);
            Ql_strcpy((char *)gprsCfg.apnPasswd, pSettings->password);
            gprsCfg.authtype = 0;

            ret = Ql_GPRS_Config(PDP_CONTXT_ID, &gprsCfg);
            if (GPRS_PDP_SUCCESS == ret) {
                OUT_DEBUG_8("Ql_GPRS_Config(apn=\"%s\", login=\"%s\", password=\"%s\"): OK\r\n",
                            pSettings->apn, pSettings->login, pSettings->password);
                m_udp_state = STATE_GPRS_ACTIVATE;
            }
            else {
                OUT_DEBUG_1("Ql_GPRS_Config(apn=\"%s\", login=\"%s\", password=\"%s\") = %d error.\r\n",
                            pSettings->apn, pSettings->login, pSettings->password, ret);
                m_udp_state = STATE_NW_QUERY_STATE;
                setGprsLastError(GPRS_CHANNEL_ERROR);
                return ret;
            }            
            break;
        }
        case STATE_GPRS_ACTIVATE:
        {
            m_udp_state = STATE_GPRS_ACTIVATING;
            ret = Ql_GPRS_Activate(PDP_CONTXT_ID);
            if (ret == GPRS_PDP_SUCCESS)
            {
                OUT_DEBUG_8("Ql_GPRS_Activate(): OK\r\n");
                m_udp_state = STATE_GPRS_GET_DNSADDRESS;
            }
            else if (ret == GPRS_PDP_WOULDBLOCK)
            {
                 OUT_DEBUG_8("Ql_GPRS_Activate(): wait callback\r\n");
                //waiting Callback_GPRS_Actived
            }
            else if (ret == GPRS_PDP_ALREADY)
            {
                OUT_DEBUG_8("Ql_GPRS_Activate(): already actived\r\n");
                m_udp_state = STATE_GPRS_GET_DNSADDRESS;
            }
            else//error
            {
                OUT_DEBUG_1("Ql_GPRS_Activate() = %d error\r\n",ret);
                m_udp_state = STATE_NW_QUERY_STATE;
                setGprsLastError(GPRS_CHANNEL_ERROR);
                return ret;
            }
            break;
        }
        case STATE_GPRS_GET_DNSADDRESS:
        {
            m_udp_state = STATE_GPRS_GET_LOCALIP;

//          uncomment this section and remove "m_udp_state = STATE_GPRS_GET_LOCALIP;" row above
//            u8 primaryAddr[16];
//            u8 bkAddr[16];

//            ret = Ql_GPRS_GetDNSAddress(PDP_CONTXT_ID, (u32*)primaryAddr,  (u32*)bkAddr);
//            if (ret == GPRS_PDP_SUCCESS) {
//                OUT_DEBUG_8("Ql_GPRS_GetDNSAddress(): OK +++ DNS1 <%d.%d.%d.%d>, DNS2 <%d.%d.%d.%d>\r\n",
//                            primaryAddr[0],primaryAddr[1],primaryAddr[2],primaryAddr[3],
//                            bkAddr[0],bkAddr[1],bkAddr[2],bkAddr[3]);
//                m_udp_state = STATE_GPRS_GET_LOCALIP;
//            }
//            else {
//                OUT_DEBUG_1("Ql_GPRS_GetDNSAddress() = %d error.-->\r\n", ret);
//                m_udp_state = STATE_GPRS_DEACTIVATE;
//            }
            break;
        }
        case STATE_GPRS_GET_LOCALIP:
        {
            u8 local_ip[5];
            Ql_memset(local_ip, 0, sizeof(local_ip));
            ret = Ql_GPRS_GetLocalIPAddress(PDP_CONTXT_ID, (u32 *)local_ip);
            if (ret == GPRS_PDP_SUCCESS) {
                OUT_DEBUG_8("Ql_GPRS_GetLocalIPAddress(): OK +++ Local IP <%d.%d.%d.%d>\r\n",
                            local_ip[0],local_ip[1],local_ip[2],local_ip[3]);
                /* first byte of local IP can't be equal to 0 in the VPN of vpnl.kyivstar.net */
                if (0 == local_ip[0]) {
                    m_udp_state = STATE_GPRS_DEACTIVATE;
                }
                else {
                    m_udp_state = STATE_SOC_CREATE;
                }
            }
            else {
                OUT_DEBUG_8("Ql_GPRS_GetLocalIPAddress() = %d error.\r\n",ret);
                m_udp_state = STATE_GPRS_DEACTIVATE;
            }
            break;
        }
        case STATE_SOC_CREATE:
        {
            m_socketid = Ql_SOC_Create(PDP_CONTXT_ID, SOC_TYPE_UDP);
            if (m_socketid < 0) {
                OUT_DEBUG_1("Ql_SOC_Create() = %d error.\r\n", m_socketid);
                m_udp_state = STATE_GPRS_DEACTIVATE;
                break;
            }
            OUT_DEBUG_8("Ql_SOC_Create(): OK +++ socketid=%d.\r\n", m_socketid);

            // -- bind the socket to necessary local port
            u16 local_port = Ar_SIM_currentSettings()->udp_ip_list.local_port;
            ret = Ql_SOC_Bind(m_socketid, local_port);
            OUT_DEBUG_GPRS("Ql_SOC_Bind(socketid=%d, \"UDP\", localport=%d) = %d\r\n",
                           m_socketid, local_port, ret);
            if (ret < SOC_SUCCESS) {
                m_udp_state = STATE_GPRS_DEACTIVATE;
                break;
            }
            m_udp_state = STATE_SOC_READY_TO_USE;
            setGprsLastError(GPRS_CHANNEL_OK);
            return RETURN_NO_ERRORS;
        }
        case STATE_GPRS_DEACTIVATE:
        {
            OUT_DEBUG_8("<--Deactivate GPRS.-->\r\n");
            m_udp_state = STATE_GPRS_DEACTIVATING_BY_FAIL;
            ret = Ql_GPRS_Deactivate(PDP_CONTXT_ID);
            if (GPRS_PDP_SUCCESS == ret)
            {
                OUT_DEBUG_8("Ql_GPRS_Deactivate(): OK\r\n");
                m_udp_state = STATE_NW_QUERY_STATE;
                closeSocket(m_socketid);
                setGprsLastError(GPRS_CHANNEL_ERROR);
            }
            else if (GPRS_PDP_WOULDBLOCK == ret)
            {
                OUT_DEBUG_8("Ql_GPRS_Deactivate(): wait callback\r\n");
                // wait for CallBack_GPRS_Deactived
            }
            else if (GPRS_PDP_ALREADY == ret)   // operation already in progress
            {
                OUT_DEBUG_8("Ql_GPRS_Deactivate(): deactivation already in progress\r\n");
            }
            else  // error
            {
                OUT_DEBUG_1("Ql_GPRS_Deactivate() = %d error\r\n", ret);
                m_udp_state = STATE_NW_QUERY_STATE;
                closeSocket(m_socketid);
                setGprsLastError(GPRS_CHANNEL_ERROR);
                return ret;
            }
            return RETURN_NO_ERRORS;
        }
        default:
            break;
    }

    Ar_Timer_startSingle(TMR_ActivateGPRS_Timer, GPRS_NORMAL_TIMER);
    return RETURN_NO_ERRORS;
}


/*
//case STATE_SOC_SEND:
//{
//    if (!m_remain_len)//no data need to send
//        break;

//    m_udp_state = STATE_SOC_SENDING;
//    do
//    {
//        ret = Ql_SOC_SendTo(m_socketid, m_pCurrentPos, m_remain_len, (u32)m_dest_ip, m_dest_port);
//        OUT_DEBUG_8("<--Send data,socketid=%d,number of bytes sent=%d-->\r\n",m_socketid,ret);
//        if(ret == m_remain_len)//send compelete
//        {
//            m_remain_len = 0;
//            m_pCurrentPos = NULL;

//            Ql_memset(m_send_buf,0,SEND_BUFFER_LEN);
//            m_udp_state = STATE_SOC_SEND;
//            break;
//        }
//        else if((ret <= 0) && (ret == SOC_WOULDBLOCK))
//        {
//            //waiting CallBack_socket_write, then send data;
//            break;
//        }
//        else if(ret <= 0)
//        {
//            OUT_DEBUG_8("<--Send data failure,ret=%d.-->\r\n",ret);
//            OUT_DEBUG_8("<-- Close socket.-->\r\n");
//            Ql_SOC_Close(m_socketid);//error , Ql_SOC_Close
//            m_socketid = -1;

//            m_remain_len = 0;
//            m_pCurrentPos = NULL;
//            if(ret == SOC_BEARER_FAIL)
//            {
//                m_udp_state = STATE_GPRS_DEACTIVATE;
//            }
//            else
//            {
//                m_udp_state = STATE_GPRS_GET_DNSADDRESS;
//            }
//            break;
//        }
//        else if(ret < m_remain_len)//continue send, do not send all data
//        {
//            m_remain_len -= ret;
//            m_pCurrentPos += ret;
//        }
//    }while(1);
//    break;
//}
        case STATE_DEST_ADDR_CHECK:
        {
            Ql_memset(m_dest_ip, 0, sizeof(m_dest_ip));
            Ql_memcpy(m_dest_ip, getIpAddressByIndex(Ar_SIM_currentSlot(), getChannel()->index), IPADDR_LEN);
            if (!m_dest_ip[0]) {
                OUT_DEBUG_1("Invalid target IP. Deactivate GPRS...\r\n");
                m_udp_state = STATE_GPRS_DEACTIVATE;
                break;
            }
//            u8 *dest_address = getIpAddressByIndex(Ar_SIM_currentSlot(), getChannel()->index);
//            ret = Ql_IpHelper_ConvertIpAddr(dest_address, (u32 *)m_dest_ip);
//            if(ret == SOC_SUCCESS) // ip address, xxx.xxx.xxx.xxx
//            {
                OUT_DEBUG_8("Valid target IP found +++ Target IP <%d.%d.%d.%d>\r\n",
                            m_dest_ip[0],m_dest_ip[1],m_dest_ip[2],m_dest_ip[3]);
                m_udp_state = STATE_SOC_CREATE;

//            }
//            else  //domain name
//            {
//                m_udp_state = STATE_DEST_ADDR_CHECKING;
//                ret = Ql_IpHelper_GetIPByHostName(PDP_CONTXT_ID, 0, dest_address, Callback_GetIpByName);
//                if(ret == SOC_SUCCESS) {
//                    OUT_DEBUG_8("Request IP by the domain name: OK\r\n");
//                }
//                else if(ret == SOC_WOULDBLOCK) {
//                    OUT_DEBUG_8("Request IP by the domain name: wait callback.\r\n");
//                    //waiting CallBack_getipbyname
//                }
//                else {
//                    OUT_DEBUG_8("Request IP by the domain name: error %d\r\n",ret);
//                    if(ret == SOC_BEARER_FAIL) {
//                         m_udp_state = STATE_GPRS_DEACTIVATE;
//                    }
//                    else {
//                         m_udp_state = STATE_GPRS_GET_DNSADDRESS;
//                    }
//                }
//            }
            break;
        }
*/


void Callback_GPRS_Actived(u8 contexId, s32 errCode, void* customParam)
{
    OUT_UNPREFIXED_LINE("\r\n");

    /* if too long delay occured => do nothing */
    if (STATE_GPRS_ACTIVATING != m_udp_state) {
        OUT_DEBUG_8("<--CallBack: active GPRS timed out. Do nothing.-->\r\n")
        return;
    }

    if(errCode == SOC_SUCCESS) {
        OUT_DEBUG_8("<--CallBack: active GPRS successfully.-->\r\n");
        m_udp_state = STATE_GPRS_GET_DNSADDRESS;
    }
    else {
        OUT_DEBUG_8("<--CallBack: active GPRS failure,contexid=%d,errCode=%d-->\r\n",contexId,errCode);
        m_udp_state = STATE_GPRS_DEACTIVATE;
    }
}

void CallBack_GPRS_Deactived(u8 contextId, s32 errCode, void* customParam )
{
    OUT_UNPREFIXED_LINE("\r\n");
    if (SOC_SUCCESS == errCode) {
        OUT_DEBUG_8("<--CallBack: deactived GPRS successfully.-->\r\n");
    }
    else {
        OUT_DEBUG_8("<--CallBack: deactived GPRS failure,(contexid=%d,error_cause=%d)-->\r\n",
                    contextId, errCode);
    }

    /* deactivated due to a fail during an activation process */
    if (STATE_GPRS_DEACTIVATING_BY_FAIL == m_udp_state) {
        setGprsLastError(GPRS_CHANNEL_ERROR);
    }
    /* all other reasons including manual deactivation and bearer fail error */
    else {
        setGprsLastError(GPRS_CHANNEL_WOULDBLOCK);
    }

    m_udp_state = STATE_NW_QUERY_STATE;
    closeSocket(m_socketid);
}

/*
void Callback_GetIpByName(u8 contexId, u8 requestId, s32 errCode,  u32 ipAddrCnt, u32* ipAddr)
{
    OUT_UNPREFIXED_LINE("\r\n");
    u8 i=0;
    if (errCode == SOC_SUCCESS)
    {
        OUT_DEBUG_8("<--CallBack: get ip by name successfully.-->\r\n");
        OUT_DEBUG_8("ipAddrCnt = %d\r\n", ipAddrCnt);
        for(i=0;i<ipAddrCnt;i++)
        {
            ipAddr += (i*4);
            OUT_DEBUG_8("+ Entry=%d, IP <%d.%d.%d.%d>\r\n",
                        i, ipAddr[0],ipAddr[1],ipAddr[2],ipAddr[3]);
        }
        Ql_memcpy(m_dest_ip, ipAddr, 4);
        m_udp_state = STATE_SOC_CREATE;
    }
    else
    {
        OUT_DEBUG_8("<--CallBack: get ip by name failure,(contexid=%d, requestId=%d, error=%d, ipAddrCnt=%d)-->\r\n",
                    contexId, requestId, errCode, ipAddrCnt);
        m_udp_state = STATE_GPRS_GET_DNSADDRESS;
    }
}
*/

void callback_socket_connect(s32 socketId, s32 errCode, void* customParam )
{
    OUT_UNPREFIXED_LINE("\r\n");
    OUT_DEBUG_3("ALERT: callback_socket_connect(): socketId=%d, errCode=%d, customParam=%d\r\n",
                socketId, errCode, customParam);
}

void callback_socket_close(s32 socketId, s32 errCode, void* customParam )
{
    OUT_UNPREFIXED_LINE("\r\n");
    OUT_DEBUG_3("ALERT: callback_socket_close(): socketId=%d, errCode=%d, customParam=%d\r\n",
                socketId, errCode, customParam);
}

void callback_socket_accept(s32 listenSocketId, s32 errCode, void* customParam )
{
    OUT_UNPREFIXED_LINE("\r\n");
    OUT_DEBUG_3("ALERT: callback_socket_accept(): listenSocketId=%d, errCode=%d, customParam=%d\r\n",
                listenSocketId, errCode, customParam);
}


static void _validPeer_handler(const SIMSlot sim, const s16 idx, u8 *last_ip)
{

    OUT_DEBUG_8("_validPeer_handler\r\n");
    /* stash important channel parameters */
    if (Ar_SIM_currentSlot() != sim)
        stashPultChannel();

    iStopTimerPultSession = 1;

    updatePultChannel(PCHT_GPRS_UDP, idx);

    /* remember valid peer's IP and mark the channel as BUSY */
    Ql_memcpy(last_ip, m_peer.peer_ip, IPADDR_LEN);
    setPultChannelBusy(TRUE);
    notifyConnectionValidIncoming(PCHT_GPRS_UDP);
}

typedef void (*ValidPeerHandler)(const SIMSlot sim, const s16 idx, u8 *last_ip);
static bool _isIncomingPeerValid(const u8 *peer_ip, ValidPeerHandler handler)
{
    static u8 last_ip[IPADDR_LEN] = {0};

    if (getChannel()->busy)
    {
        /* check if the peer's IP is valid for current session */
        if (0 == Ql_memcmp(peer_ip, last_ip, IPADDR_LEN))
            return TRUE;

        OUT_DEBUG_8("Attempt to connect from node \"%d.%d.%d.%d\".\r\n"
                    "Current channel is busy now by node \"%d.%d.%d.%d\"\r\n",
                    peer_ip[0], peer_ip[1], peer_ip[2], peer_ip[3],
                    last_ip[0], last_ip[1], last_ip[2], last_ip[3]);
    }
    else
    {
        /* check if the peer's IP is registered */
        SIMSlot sim;
        const s16 idx = findIpAddressIndex(peer_ip, &sim);
        if (idx > -1) {
            handler(sim, idx, last_ip);
            return TRUE;
        }

        OUT_DEBUG_3("IP address \"%d.%d.%d.%d\" was not found in DB. Rejected.\r\n",
                    peer_ip[0], peer_ip[1], peer_ip[2], peer_ip[3]);
    }

    return FALSE;
}

void callback_socket_read(s32 socketId, s32 errCode, void* customParam )
{
    OUT_UNPREFIXED_LINE("\r\n");
    OUT_DEBUG_2("callback_socket_read()\r\n")

    s32 ret = 0;
    m_peer.rx_len = 0;


    if (errCode) {
        OUT_DEBUG_1("<--CallBack: socket read failure,(sock=%d,error=%d)-->\r\n",socketId,errCode);
        reactivateGprs();
        return;
    }

    if (!Ar_System_checkFlag(SysF_DeviceInitialized))
        return;

    Ql_memset(m_peer.rx_datagram, 0, GPRS_RX_BUFFER_SIZE);

    ret = Ql_SOC_RecvFrom(socketId, m_peer.rx_datagram, GPRS_RX_BUFFER_SIZE,
                          (u32*)m_peer.peer_ip, &m_peer.peer_port);
    if ((ret < 0) && (SOC_WOULDBLOCK != ret))
    {
        OUT_DEBUG_8("<-- Receive data failure,ret=%d.-->\r\n",ret);
        reactivateGprs();
    }
    else if (SOC_WOULDBLOCK == ret)
    {
        // wait next callback_socket_read()
    }
    else if (0 != ret)
    {
        if (!_isIncomingPeerValid(m_peer.peer_ip, &_validPeer_handler))
            return;


        /* in configuring state only data from configurator can be received */
//         if (Ar_System_checkFlag(SysF_DeviceConfiguring))
//         {
//             if( !(CONFIGURATION_PACKET_MARKER == m_peer.rx_datagram[getMessageBuilder(NULL)->codec->SPSMarkerPosition()] ||
//                    ( (PMQ_AuxInfoMsg == (m_peer.rx_datagram[getMessageBuilder(NULL)->codec->SPSAuxInfoPosition()] >> 4)) &&
//                      (PMC_ArmorAck == (m_peer.rx_datagram[getMessageBuilder(NULL)->codec->SPSACKPosition()]))) ) )
//             {
//                 OUT_DEBUG_GPRS("callback_socket_read - NO configuratot pkt IN SysF_DeviceConfiguring mode! \r\n");
//                return;
//             }
//         }
//        if (Ar_System_checkFlag(SysF_DeviceConfiguring) &&
//                CONFIGURATION_PACKET_MARKER != m_peer.rx_datagram[getMessageBuilder(NULL)->codec->SPSMarkerPosition()])
//        {
//            OUT_DEBUG_GPRS("@@@@@ MarkerPosition = %d, value = %X \r\n",
//                            getMessageBuilder(NULL)->codec->SPSMarkerPosition(),
//                           m_peer.rx_datagram[getMessageBuilder(NULL)->codec->SPSMarkerPosition()]);
//            return;
//        }

        m_peer.rx_len = ret;

        OUT_DEBUG_GPRS("UDP: received %d byte(s) from %d.%d.%d.%d:%d through socket %d\r\n",
                        m_peer.rx_len,
                        m_peer.peer_ip[0],m_peer.peer_ip[1],m_peer.peer_ip[2],m_peer.peer_ip[3],
                        m_peer.peer_port,
                        socketId);

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        // -- WARNING: for debug purposes only, delete in release code
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    Ar_Helper_debugOutDataPacket(m_peer.rx_datagram, m_peer.rx_len);
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        /*good news: a message to the main task can be sent from the main task */
//        ret = Ql_OS_SendMessage(main_task_id, MSG_ID_GPRS_RX, (u32)&m_peer.rx_datagram, m_peer.rx_len);
//        if (ret != OS_SUCCESS) { OUT_DEBUG_1("Ql_OS_SendMessage() = %d error\r\n", ret); }

        ret = getMessageBuilder(NULL)->readUdpDatagram(&m_peer);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("Codec %s: readUdpDatagram() = %d error\r\n", getCodec_humanize(), ret);
        }
    }
}


void callback_socket_write(s32 socketId, s32 errCode, void* customParam )
{
    OUT_UNPREFIXED_LINE("\r\n");
    OUT_DEBUG_3("ALERT: callback_socket_write(): socketId=%d, errCode=%d, customParam=%d\r\n",
                socketId, errCode, customParam);

    s32 ret = 0;

    if (errCode) {
        OUT_DEBUG_1("<--CallBack: socket write failure,(sock=%d,error=%d)-->\r\n",socketId,errCode);
        reactivateGprs();
        return;
    }

    u8 *dest_ip = 0;
    u16 dest_port = 0;

    ret = getUdpDestination(&dest_ip, &dest_port);

    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("getUdpDestination() = %d error\r\n", ret);
        return;
    }


    do {
        const s32 lenToSend = m_lenToUpdSend;

        const s32 sent = Ql_SOC_SendTo(m_socketid,
                                       getMessageBuilder(0)->codec->txBuffer,
                                       lenToSend,
                                       (u32)dest_ip,
                                       dest_port);

        if (sent == lenToSend) {
            OUT_DEBUG_3("Sent %d byte(s) to %d.%d.%d.%d:%d\r\n",
                        sent, dest_ip[0],dest_ip[1],dest_ip[2],dest_ip[3], dest_port);
            break;
        }
        else if (sent < 0 && SOC_WOULDBLOCK == sent) {
            OUT_DEBUG_1("Ql_SOC_SendTo(socketid=%d, datalen=%d, dest=%d.%d.%d.%d:%d): wait callback\r\n",
                        m_socketid, lenToSend, dest_ip[0],dest_ip[1],dest_ip[2],dest_ip[3], dest_port);
            break;
        }
        else if (sent < 0) {
            OUT_DEBUG_1("<--Send data failure,ret=%d.-->\r\n", sent);
            reactivateGprs();
            break;
        }
        else if (sent < lenToSend) {
            continue;
        }
     } while (1);
}



/*****************************************************************
* Functions
******************************************************************/
void activateGprs_timerHandler(void *data)
{
    OUT_DEBUG_3("Timer #%s# is timed out\r\n", Ar_Timer_getTimerNameByCode(TMR_ActivateGPRS_Timer));

    s32 ret = _GprsActivation_Step();
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("_GprsActivation_Step() = %d error\r\n", ret);
    }
}

s32 preinitGprs(void)
{
    OUT_DEBUG_2("preinitGprs()\r\n");

    s32 ret = Ql_GPRS_Register(PDP_CONTXT_ID, &callback_gprs_func, NULL);
    if (GPRS_PDP_ALREADY == ret) {
        OUT_DEBUG_3("Ql_GPRS_Register(): GPRS callbacks already registered\r\n")
    }
    else if (ret != GPRS_PDP_SUCCESS) {
        OUT_DEBUG_1("Ql_GPRS_Register() = %d error\r\n", ret);
        return ret;
    }

    ret = Ql_SOC_Register(callback_soc_func, NULL);
    if (SOC_ALREADY == ret) {
        OUT_DEBUG_3("Ql_SOC_Register(): SOC callbacks already registered\r\n")
    }
    else if (ret != SOC_SUCCESS) {
        OUT_DEBUG_1("Ql_SOC_Register() = %d error\r\n", ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}

bool isGprsReady(void)
{ return STATE_SOC_READY_TO_USE == m_udp_state; }

//GprsPeer *getGprsPeer(void)
//{ return &m_peer; }

s32 getGprsLastError(void)
{ return m_last_error; }

void setGprsLastError(Enum_GPRS_CHANNEL_Error error)
{ m_last_error = error; }

s32 fa_activateGprs(void)
{
    OUT_DEBUG_2("fa_activateGprs()\r\n");

    if (STATE_NW_QUERY_STATE == m_udp_state) {
        m_activate_iteration_counter = 0;
        setGprsLastError(GPRS_CHANNEL_WOULDBLOCK);
        Ar_Timer_startSingle(TMR_ActivateGPRS_Timer, 1);
    }
    return getGprsLastError();
}

void deactivateGprs(void)
{
    OUT_DEBUG_2("deactivateGprs()\r\n");

    Ar_Timer_stop(TMR_ActivateGPRS_Timer);

    if (STATE_NW_QUERY_STATE == m_udp_state)
    {
        ; // already deactived => do nothing
    }
    else if (m_udp_state > STATE_GPRS_ACTIVATING_BEGIN &&
             m_udp_state <= STATE_GPRS_ACTIVATING_END)
    {
        m_udp_state = STATE_GPRS_DEACTIVATING_MANUALLY;
        s32 ret = Ql_GPRS_Deactivate(PDP_CONTXT_ID);
        if (GPRS_PDP_SUCCESS == ret)
        {
            OUT_DEBUG_8("Ql_GPRS_Deactivate(): OK\r\n");
            m_udp_state = STATE_NW_QUERY_STATE;
            closeSocket(m_socketid);
            setGprsLastError(GPRS_CHANNEL_WOULDBLOCK);
        }
        else if (GPRS_PDP_WOULDBLOCK == ret)
        {
            OUT_DEBUG_8("Ql_GPRS_Deactivate(): wait callback\r\n");
            // wait for CallBack_GPRS_Deactived
        }
        else if (GPRS_PDP_ALREADY == ret)
        {
            OUT_DEBUG_8("Ql_GPRS_Deactivate(): deactivation already in progress\r\n");
        }
        else  // error
        {
            OUT_DEBUG_1("Ql_GPRS_Deactivate() = %d error\r\n", ret);
            m_udp_state = STATE_NW_QUERY_STATE;
            closeSocket(m_socketid);
            setGprsLastError(GPRS_CHANNEL_ERROR);
        }
    }
    else
    {
        OUT_DEBUG_3("Wait... GPRS deactivation is in progress already\r\n");
    }
}

void reactivateGprs(void)
{
    OUT_DEBUG_2("reactivateGprs()\r\n");
    m_udp_state = STATE_NW_QUERY_STATE;
    closeSocket(m_socketid);
    fa_activateGprs();
}

void closeSocket(s32 socketId)
{
    if ((s32)SOCKET_INIT_VALUE == socketId)
        return;

    OUT_DEBUG_2("closeSocket(socketid=%d)\r\n", socketId);

    s32 ret = Ql_SOC_Close(socketId);
    if (ret < SOC_SUCCESS)
        OUT_DEBUG_1("closeSocket(): Ql_SOC_Close(socketid=%d) = %d error\r\n", socketId, ret);
    socketId = SOCKET_INIT_VALUE;
}


s32 getUdpDestination(u8 *ip[], u16 *port)
{
    /* if communicate with the configurator directly or remotely */
    if (CCM_NotConnected != Ar_Configurator_ConnectionMode())
    {
        *port = m_peer.peer_port;
        *ip = m_peer.peer_ip;
        if (!ip || !ip[0]) {
            OUT_DEBUG_1("Invalid dest IP\r\n");
            return ERR_INVALID_DEST_IP;
        }
    }
    /* if communicate with the pult normally */
    else if (PCHT_GPRS_UDP == getChannel()->type)
    {
        *port = Ar_SIM_currentSettings()->udp_ip_list.dest_port;
        *ip = getIpAddressByIndex(Ar_SIM_currentSlot(), getChannel()->index);
        if (!ip || !ip[0]) {
            OUT_DEBUG_1("Dest IP not found\r\n");
            return ERR_INVALID_DEST_IP;
        }
    }
    else
    {
        OUT_DEBUG_1("Invalid pult channel type %s\r\n",
                    getChannelType_humanize());
        return ERR_INVALID_PULT_CHANNEL_TYPE;
    }

    return RETURN_NO_ERRORS;
}

/*
 * Returns the number of bytes actually sent or an error message
 */
s32 sendByUDP(u8 *data, u16 len)
{
    OUT_DEBUG_2("sendByUDP(datalen=%d)\r\n", len);

    u8 *dest_ip = 0;
    u16 dest_port = 0;

    m_lenToUpdSend = len;

    s32 ret = getUdpDestination(&dest_ip, &dest_port);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("getUdpDestination() = %d error\r\n", ret);
        return ret;
    }

    s32 sent = Ql_SOC_SendTo(m_socketid, data, m_lenToUpdSend, (u32)dest_ip, dest_port);

    if (SOC_WOULDBLOCK == sent) {
        OUT_DEBUG_1("Ql_SOC_SendTo(socketid=%d, datalen=%d, dest=%d.%d.%d.%d:%d): wait callback\r\n",
                    m_socketid, m_lenToUpdSend, dest_ip[0],dest_ip[1],dest_ip[2],dest_ip[3], dest_port);
        return RETURN_NO_ERRORS;
    }
    // an error has occured => need to restart connection
    else if (sent < 0) {
        OUT_DEBUG_1("Ql_SOC_SendTo(socketid=%d, datalen=%d, dest=%d.%d.%d.%d:%d) = %d error\r\n",
                    m_socketid, m_lenToUpdSend, dest_ip[0],dest_ip[1],dest_ip[2],dest_ip[3], dest_port, sent);
        reactivateGprs();
        return ERR_CHANNEL_ESTABLISHING_WOULDBLOCK;
    }

    OUT_DEBUG_3("Sent %d byte(s) to %d.%d.%d.%d:%d through socket %d\r\n",
                sent, dest_ip[0],dest_ip[1],dest_ip[2],dest_ip[3], dest_port, m_socketid);

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        // -- WARNING: for debug purposes only, delete in release code
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    Ar_Helper_debugOutDataPacket(data, len);
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    return sent;
}

const char *gprsUdpStateByCode(u8 code)
{
    switch (code) {
    CASE_RETURN_NAME(STATE_NW_QUERY_STATE);
    CASE_RETURN_NAME(STATE_GPRS_CONFIG);
    CASE_RETURN_NAME(STATE_GPRS_ACTIVATE);
    CASE_RETURN_NAME(STATE_GPRS_ACTIVATING);
    CASE_RETURN_NAME(STATE_GPRS_GET_DNSADDRESS);
    CASE_RETURN_NAME(STATE_GPRS_GET_LOCALIP);
    CASE_RETURN_NAME(STATE_DEST_ADDR_CHECKING);
    CASE_RETURN_NAME(STATE_SOC_CREATE);
    CASE_RETURN_NAME(STATE_SOC_READY_TO_USE);
    CASE_RETURN_NAME(STATE_GPRS_DEACTIVATE);
    CASE_RETURN_NAME(STATE_GPRS_DEACTIVATING_BY_FAIL);
    CASE_RETURN_NAME(STATE_GPRS_DEACTIVATING_MANUALLY);
    DEFAULT_RETURN_CODE(code);
    }
}
