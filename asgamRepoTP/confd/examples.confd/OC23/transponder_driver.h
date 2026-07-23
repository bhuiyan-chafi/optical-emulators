#ifndef _TRANSPONDER_DRIVER_H_INCLUDED_
  #define _TRANSPONDER_DRIVER_H_INCLUDED_

void lch_set_admin_state(uint32_t lch, char const* const val);

void component_set_target_power(char const* const name, char const* const val);

void component_set_frequency(char const* const name, char const* const val);

void component_set_operational_mode(char const* const name, char const* const val);

int sender(char *type, char *path, char *resp, int portno, char *host);

int string_splitter(char ret_str[3][100], char * input_str, char * delimiter_str);

void intHardwareCommunication();

void stopHardwareCommunication();

void error(const char *msg);

void bgp_config(uint32_t type, char const* const las, char const* const ras, char const* const ip, char const* const intf);

void intf_config(uint32_t type, char const* const ip, char const* const mask, char const* const intf, char const* const speed);
#endif
