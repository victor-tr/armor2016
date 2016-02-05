TEMPLATE = app
CONFIG += console
CONFIG -= qt core

#unix{
#INCLUDEPATH += /home/rem/Dvlp/OpenCPU_GS2_SDK_V5.0/include
#INCLUDEPATH += /home/rem/Dvlp/OpenCPU_GS2_SDK_V5.0/ril/inc
#DEPENDPATH += $${INCLUDEPATH}
#}

#win32{
INCLUDEPATH += $${PWD}/../include
INCLUDEPATH += $${PWD}/../ril/inc
INCLUDEPATH += $${PWD}/config
INCLUDEPATH += "$$(SYSTEMDRIVE)/Program Files/ARM/RVCT/Data/3.1/569/include"
INCLUDEPATH += $$(HOME)/.wine/drive_c/MentorGraphics/Sourcery_CodeBench_Lite_for_ARM_EABI/lib/gcc/arm-none-eabi/4.8.1/include
INCLUDEPATH += $$(HOME)/.wine/drive_c/MentorGraphics/Sourcery_CodeBench_Lite_for_ARM_EABI/arm-none-eabi/include
DEPENDPATH += $${INCLUDEPATH}
#}


DEFINES += __CUSTOMER_CODE__


SOURCES += \
    db/fs.c \
    db/db.c \
    db/db_serv.c \
    db/lexicon.c \
    \
    core/main.c \
    core/system.c \
    core/atcommand.c \
    core/simcard.c \
    core/canbus.c \
    core/uart.c \
    core/mcu_rx_buffer.c \
    core/mcu_tx_buffer.c \
    core/power.c    \
    \
    config/custom_sys_cfg.c \
    config/sys_config.c \
    \
    pult/connection.c \
    pult/sms.c \
    pult/voicecall.c \
    pult/gprs_udp.c \
    pult/pult_tx_buffer.c \
    pult/pult_rx_buffer.c \
    pult/protocol_base_armor.c \
    pult/pult_rx_prebuffer_armor.c \
    \
    service/crc.c \
    service/humanize.c \
    service/syscalls.c \
    service/helper.c \
    \
    codecs/codecs.c \
    codecs/codec_armor.c \
    codecs/codec_lun_9t.c \
    codecs/codec_lun_9s.c \
    \
    states/armingstates.c \
    states/armingstate_armed.c \
    states/armingstate_armed_ing.c \
    states/armingstate_configuring.c \
    states/armingstate_disarmed.c \
    states/armingstate_disarmed_ing.c \
    states/armingstate_disarmed_forced.c \
    states/armingstate_armed_partial.c \
    \
    events/event_key.c \
    events/event_internal_timer.c \
    events/event_button.c \
    events/event_zone.c \
    \
    configurator/configurator_tx_buffer.c \
    configurator/configurator_rx_buffer.c \
    configurator/configurator.c \
    common/debug_macro.c \
    core/timers.c \
    core/urc.c \
    pult/connection_voicecall.c \
    pult/connection_udp.c \
    pult/connection_sms.c \
    pult/protocol_gprs_armor.c \
    pult/pult_requests.c \
    events/event_group.c



HEADERS += \
    config/custom_feature_def.h \
    config/custom_gpio_cfg.h \
    config/custom_heap_cfg.h \
    config/custom_task_cfg.h \
    \
    db/fs.h \
    db/db.h \
    db/db_serv.h \
    db/db_macro.h \
    db/lexicon.h \
    \
    common/errors.h \
    common/debug_macro.h \
    common/defines.h \
    common/configurator_protocol.h \
    common/fifo_macro.h \
    \
    service/crc.h \
    service/humanize.h \
    service/helper.h \
    \
    pult/connection.h \
    pult/voicecall.h \
    pult/sms.h \
    pult/gprs_udp.h \
    pult/pult_tx_buffer.h \
    pult/pult_rx_buffer.h \
    \
    codecs/codecs.h \
    codecs/message_codes.h \
    \
    states/armingstates.h \
    \
    core/system.h \
    core/atcommand.h \
    core/simcard.h \
    core/canbus.h \
    core/uart.h \
    core/mcudefinitions.h \
    core/mcu_rx_buffer.h \
    core/mcu_tx_buffer.h \
    core/power.h \
    \
    events/event_zone.h \
    events/events.h \
    events/event_internal_timer.h \
    events/event_key.h \
    events/event_button.h \
    \
    configurator/configurator_tx_buffer.h \
    configurator/configurator_rx_buffer.h \
    configurator/configurator.h \
    common/circular_buffer.h \
    codecs/codec_armor.h \
    codecs/codec_lun_9t.h \
    pult/protocol_base_armor.h \
    pult/pult_rx_prebuffer_armor.h \
    core/timers.h \
    core/urc.h \
    pult/connection_voicecall.h \
    pult/connection_udp.h \
    pult/connection_sms.h \
    pult/protocol_gprs_armor.h \
    pult/pult_requests.h \
    events/event_group.h




OTHER_FILES += \
    makefile

