If you are designing or building a Digital Twin ROADM based on OpenConfig (similar to how the Nokia CHM6 transponder digital twin was built with ConfD and OpenConfig   
  models), you should avoid proprietary <property> strings like MG_ON_*.                                                                                                  
                                                                                                                                                                          
  Instead, industry-standard OpenConfig provides three standard architectural pillars to differentiate port types cleanly, deterministically, and in a vendor-neutral way:
  ──────                                                                                                                                                                  
  ### 1. The OpenConfig Standard Port Identity: OPTICAL_PORT_TYPE                                                                                                         
                                                                                                                                                                          
  The primary OpenConfig standard for classifying optical ports is defined in openconfig-transport-types.yang:1758-1815.                                                  
                                                                                                                                                                          
  In your digital twin's physical inventory (openconfig-platform), each port component should use the standard OpenConfig OPTICAL_PORT_TYPE identities:                   
                                                                                                                                                                          
   Standard OpenConfig Identity    │ Port Role                       │ Description
  ─────────────────────────────────┼─────────────────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────────
   oc-opt-types:ADD                │ Add Port                        │ Local tributary port injecting a wavelength from a transponder into the ROADM.
   oc-opt-types:DROP               │ Drop Port                       │ Local tributary port dropping a wavelength from the ROADM to a transponder.
   oc-opt-types:INGRESS            │ Line Degree RX                  │ WDM line multiplex port receiving multi-wavelength spectrum from a neighboring ROADM / fiber span.
   oc-opt-types:EGRESS             │ Line Degree TX                  │ WDM line multiplex port transmitting multi-wavelength spectrum out to the fiber span.
   oc-opt-types:MONITOR            │ OCM Port                        │ Optical Channel Monitor tap port.
                                                                                                                                                                          
  #### How this looks in ConfD / NETCONF XML:                                                                                                                             
                                                                                                                                                                          
    <!-- Example: Add Port -->                                                                                                                                            
    <component>                                                                                                                                                           
      <name>port-add-1</name>                                                                                                                                             
      <state>                                                                                                                                                             
        <name>port-add-1</name>                                                                                                                                           
        <type xmlns:oc-platform-types="http://openconfig.net/yang/platform-types">oc-platform-types:PORT</type>                                                           
        <optical-port-type xmlns:oc-opt-types="http://openconfig.net/yang/transport-types">oc-opt-types:ADD</optical-port-type>                                           
      </state>                                                                                                                                                            
    </component>                                                                                                                                                          
                                                                                                                                                                          
    <!-- Example: Line Degree 1 Ingress Port -->                                                                                                                          
    <component>                                                                                                                                                           
      <name>port-deg1-rx</name>                                                                                                                                           
      <state>                                                                                                                                                             
        <name>port-deg1-rx</name>                                                                                                                                         
        <type xmlns:oc-platform-types="http://openconfig.net/yang/platform-types">oc-platform-types:PORT</type>                                                           
        <optical-port-type xmlns:oc-opt-types="http://openconfig.net/yang/transport-types">oc-opt-types:INGRESS</optical-port-type>                                       
      </state>                                                                                                                                                            
    </component>                                                                                                                                                          
  ──────                                                                                                                                                                  
  ### 2. The Functional Schema Hierarchy: openconfig-wavelength-router                                                                                                    
                                                                                                                                                                          
  OpenConfig standardizes ROADMs by grouping degree ports into a dedicated container called <degrees> within openconfig-wavelength-router:                                
                                                                                                                                                                          
  1. Line Multiplex Ports (Degrees):                                                                                                                                      
  In OpenConfig, every line degree (direction) explicitly references its line ports via in-port and out-port:                                                             
    <wavelength-router xmlns="http://openconfig.net/yang/wavelength-router">                                                                                              
      <degrees>                                                                                                                                                           
        <degree>                                                                                                                                                          
          <degree-number>1</degree-number>                                                                                                                                
          <state>                                                                                                                                                         
            <degree-number>1</degree-number>                                                                                                                              
            <in-port>port-deg1-rx</in-port>    <!-- Deterministically marks this as Line Degree 1 In -->                                                                  
            <out-port>port-deg1-tx</out-port>  <!-- Deterministically marks this as Line Degree 1 Out -->                                                                 
          </state>                                                                                                                                                        
        </degree>                                                                                                                                                         
        <degree>                                                                                                                                                          
          <degree-number>2</degree-number>                                                                                                                                
          <state>                                                                                                                                                         
            <degree-number>2</degree-number>                                                                                                                              
            <in-port>port-deg2-rx</in-port>                                                                                                                               
            <out-port>port-deg2-tx</out-port>                                                                                                                             
          </state>                                                                                                                                                        
        </degree>
      </degrees>
    </wavelength-router>
  Why this is standard: Any SDN controller reading standard OpenConfig inspects <degrees>: if a port is in <degrees>, it is a Line Multiplex (Degree) port. Any port not  
  in <degrees> is an Add/Drop tributary.
  2. Add/Drop Cross-Connects (<media-channels>):
  When lightpaths are routed through the ROADM, media channels reference the source and destination ports:
      • Add flow: <source><port-name>port-add-1</port-name></source> → <dest><port-name>port-deg1-tx</port-name></dest>
      • Drop flow: <source><port-name>port-deg1-rx</port-name></source> → <dest><port-name>port-drop-1</port-name></dest>
      • Express pass-through: <source><port-name>port-deg1-rx</port-name></source> → <dest><port-name>port-deg2-tx</port-name></dest>
  
  ──────
  ### 3. Clear Port Naming & Indexing (Best Practice)
  
  Just as the Nokia CHM6 digital twin (init_chm6_config.xml) established consistent port names (port-1 to port-16 for client, port-17 to port-24 for line), a ROADM       
  digital twin should adopt a predictable naming convention:
  
  • Line Degree Ports:
      • deg1-rx / deg1-tx (or line-1-in / line-1-out)
      • deg2-rx / deg2-tx (or line-2-in / line-2-out)
  • Add/Drop Tributary Ports:
      • add-1, add-2, ..., add-N
      • drop-1, drop-2, ..., drop-N
  
  ──────
  ### Summary Checklist for your ROADM Digital Twin
  
  If you implement the digital twin with these three elements:
  
  1. Set state/optical-port-type to oc-opt-types:ADD, DROP, INGRESS, or EGRESS.
  2. Declare your line degrees in <wavelength-router><degrees> with <in-port> and <out-port>.
  3. Use structured port names (e.g., deg1-rx, add-1).
  
  Any standard SDN controller or WebUI parser can immediately identify port types by schema definition without parsing vendor-specific text strings.

