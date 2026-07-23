# Author

```
ASM CHAFIULLAH BHUIYAN
Thesis student @UniPi
Supervisor: Prof. Alessio Giorgetti
```
## Files
- download the file confd files from [google drive](https://drive.google.com/drive/folders/1wuYBpeoCnLQXNSaiPmKBq4volhRPVgLq?usp=sharing)

## About

The primary goal was to develop 3 optical emulators to mimic the behavior of openconfig type transponder and roadm devices, openroadm type roadm devices. I already had these emulators from my professors, but to conduct our research on RSA, we needed to build new ones with some modifications.

For openconfig, openroadm terminal-devices and roadms I followed the docker images of professor alessio and andrea. But I have build these new emulator using the latest version of confd(confd 8) and and openconfig, openroadm yang models. Maybe in future these versions will change, so if anyone is following this and want to be in the same page, then check this [folder](/repos/) for the models I used. The emulators from my professors are not shared here, because I don't hold the authority to share them. But if anyone is interested, then they can contact them for the access. 

## Primary Knowledge

For me, I had some previous knowledge about these terminologies but if anyone is interested for a quick recap, then please go through these.

1. Prepare a virtual environment first for the testing and then jump into theoretical studies. Follow these [steps](ENV.md) for the virtual environment. 

2. About `NETCONF`, read [this](./knowledge/NETCONF.md).

3. About `CONFD` read [this](./knowledge/CONFD.md).

4. About `YANG` read [this](./knowledge/YANG.md).

5. About `OPENCONFIG` read [this](./knowledge/OPENCONFIG.md).

## OpenConfig Transponder/Muxponder/Terminal-Device Emulator

The contents are located in this [folder](./emulator_TP_conf8/). If you are trying to build this emulator from scratch, then follow the [REPORT](./emulator_TP_conf8/REPORT.md). But if you are have loaded the image from the docker hub then follow this:

