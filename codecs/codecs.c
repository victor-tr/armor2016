#include "codecs.h"
#include "codec_lun_9t.h"
#include "codec_armor.h"

#include "pult/pult_tx_buffer.h"
#include "pult/gprs_udp.h"
#include "pult/voicecall.h"
#include "pult/protocol_base_armor.h"
#include "pult/protocol_gprs_armor.h"

#include "service/crc.h"
#include "service/humanize.h"

#include "configurator/configurator.h"

#include "states/armingstates.h"

#include "core/timers.h"



static PultMessageCodec _codec;


//
void registerProtocol(PultChannelType type, IProtocol *protocol)
{ _codec.protocols[type] = protocol; }

IProtocol *getProtocol(PultChannelType type)
{ return _codec.protocols[type]; }


//
static s32 send(void)
{
    OUT_DEBUG_2("msg_builder::send() through %s\r\n", getChannelType_humanize());

    s32 ret = _codec.sends[getChannel()->type]();
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("codec::sends[%s]() = %d error\r\n", getChannelType_humanize(), ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}

static s32 readUdpDatagram(GprsPeer *peer)
{
    const u8 marker = peer->rx_datagram[SPS_MARKER];

    // ****************************************************************************************
    //    sample code for remote configuring
    // ****************************************************************************************
    if ((CONFIGURATION_PACKET_MARKER == marker ||
         CONFIGURATION_AUTOSYNC_PACKET_MARKER == marker) &&
            checkConfiguratorCRC(peer->rx_datagram))
    {
        OUT_DEBUG_2("msg_builder::readUdpDatagram() through GPRS UDP\r\n");

//        Ql_memcpy(rx_buffer, gprs_read_buffer, gprs_received_bytes_qty);
//        rxMsgLen = gprs_received_bytes_qty;

        // -- don't parse until entire pkt isn't received
//        u16 len = rx_buffer[SPS_BYTES_QTY_H] << 8 | rx_buffer[SPS_BYTES_QTY_L];
//        if (len < rxMsgLen - SPS_PREF_N_SUFF)
//            return RETURN_NO_ERRORS;

        RxConfiguratorPkt pkt;

        pkt.bAppend =       peer->rx_datagram[SPS_APPEND_FLAG];
        pkt.data =         &peer->rx_datagram[SPS_PREFIX];
        pkt.datalen =       peer->rx_datagram[SPS_BYTES_QTY_H] << 8 | peer->rx_datagram[SPS_BYTES_QTY_L];
        pkt.dataType =      (DbObjectCode)peer->rx_datagram[SPS_FILECODE];
        pkt.marker =        peer->rx_datagram[SPS_MARKER];
        pkt.pktIdx =        peer->rx_datagram[SPS_PKT_INDEX];
        pkt.repeatCounter = peer->rx_datagram[SPS_PKT_REPEAT];
        pkt.sessionKey =    peer->rx_datagram[SPS_PKT_KEY];

        s32 ret = Ar_Configurator_processSettingsData(&pkt, CCM_RemoteModemConnection);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("Ar_Configurator_processSettingsData() = %d error.\r\n", ret);
        }

        return RETURN_NO_ERRORS;
    }
    // ****************************************************************************************

    OUT_DEBUG_2("msg_builder::readUdpDatagram() through %s\r\n", getChannelType_humanize());

    s32 ret = _codec.readUdp(peer->rx_datagram, peer->rx_len);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("codec::readUdp() = %d error\r\n", ret);
        return ret;
    }
    OUT_DEBUG_8("UDP>>> received %d bytes\r\n", peer->rx_len);
    if (!_codec.notifyUdpDatagramReceived)
        return ERR_OPERATION_NOT_IMPLEMENTED;
    _codec.notifyUdpDatagramReceived(peer->rx_datagram, peer->rx_len);
    return RETURN_NO_ERRORS;
}

static s32 readDtmfCode(u8 dtmfCode)
{
    OUT_DEBUG_2("msg_builder::readDtmfCode() through %s\r\n", getChannelType_humanize());

    s32 digit = _codec.readDtmf(dtmfCode);
    if (digit < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("codec::readDtmf() = %d error\r\n", digit);
        return digit;
    }
    OUT_DEBUG_8("DTMF>>> decoded: %d\r\n", digit);
    if (!_codec.notifyDtmfDigitReceived)
        return ERR_OPERATION_NOT_IMPLEMENTED;
    _codec.notifyDtmfDigitReceived(digit);
    return RETURN_NO_ERRORS;
}

void init_pultMessageBuilder(PultMessageBuilder *builder, PultMessageCodecType codecType)
{
    builder->codec = &_codec;
    builder->txFifo = &pultTxBuffer;
    builder->send = &send;
    builder->readUdpDatagram = &readUdpDatagram;
    builder->readDtmfCode = &readDtmfCode;

    switch (codecType) {
    case PMCT_CODEC_LT9:
        //init_codec_lun9t(&_codec);
        break;
    case PMCT_CODEC_LS9:

        break;
    case PMCT_CODEC_A:
    default:
        init_codec_armor(&_codec);
        init_pult_protocol_dtmf_armor(builder);
        init_pult_protocol_udp_armor(builder);
        break;
    }

    OUT_DEBUG_2("init_pultMessageBuilder(): codec %s\r\n", getCodec_humanize());
}
