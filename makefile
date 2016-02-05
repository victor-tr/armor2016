############################################
## Here, define your include directories  ##
############################################
INCLUDES =  -I .\\  -I..\ril\inc\\  -I..\include\\ -I ./config -I $(DIR_ARMINC)


############################################
## Here, define source files to compile   ##
############################################
OBJS= config\sys_config.o     \
      config\custom_sys_cfg.o \
      \
      common\debug_macro.o \
	  \
    core\main.o \
    core\system.o \
    core\uart.o \
    core\atcommand.o \
    core\simcard.o \
    core\canbus.o \
    core\mcu_tx_buffer.o \
    core\mcu_rx_buffer.o \
    core\power.o \
    core\timers.o \
    core\urc.o \
    \
    service\crc.o \
    service\humanize.o \
    service\helper.o \
    \
    db\fs.o \
    db\db.o \
    db\db_serv.o \
    db\lexicon.o \
    \
    events\event_zone.o \
    events\event_key.o \
    events\event_button.o \
    events\event_internal_timer.o \
    events\event_group.o \
    \
    states\armingstates.o \
    states\armingstate_armed.o \
    states\armingstate_armed_partial.o \
    states\armingstate_armed_ing.o \
    states\armingstate_configuring.o \
    states\armingstate_disarmed.o \
    states\armingstate_disarmed_ing.o \
    states\armingstate_disarmed_forced.o \
    \
    configurator\configurator_tx_buffer.o \
    configurator\configurator_rx_buffer.o \
    configurator\configurator.o \
    \
    pult\connection.o \
    pult\connection_voicecall.o \
    pult\connection_udp.o \
    pult\connection_sms.o \
    pult\gprs_udp.o \
    pult\sms.o \
    pult\voicecall.o \
    pult\pult_tx_buffer.o \
    pult\pult_rx_buffer.o \
    pult\pult_rx_prebuffer_armor.o \
    pult\protocol_base_armor.o \
    pult\protocol_gprs_armor.o \
    pult\pult_requests.o \
    \
    codecs\codecs.o \
    codecs\codec_lun_9t.o \
    codecs\codec_lun_9s.o \
    codecs\codec_armor.o \



##################################################
## Here, define your subdirectories to compile  ##
##################################################
SUBPATHS= config \
        core \
        common \
        db \
        states \
        pult \
        codecs  \
        events \
        configuring \
        service \
        configurator \



all:mksubpaths  listobj $(OBJS)

include ..\make\rvct\makefiledef
