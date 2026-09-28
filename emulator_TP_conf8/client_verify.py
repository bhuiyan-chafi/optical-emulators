from ncclient import manager
import xml.etree.ElementTree as ET

# Connection details
HOST = 'localhost'
PORT = 2024
USER = 'admin'
PASS = 'admin'

def get_ports_and_modes():
    print(f"Connecting to {HOST}:{PORT}...")
    try:
        with manager.connect(host=HOST, port=PORT, username=USER, password=PASS,
                             hostkey_verify=False, look_for_keys=False, allow_agent=False) as m:
            
            print("Connected! Fetching configuration...")
            
            # Split into two calls to observe single-root constraint in response parsing
            
            # 1. Fetch Components
            print("Fetching components...")
            filter_comps = """
            <components xmlns="http://openconfig.net/yang/platform">
                <component/>
            </components>
            """
            reply_comp = m.get_config(source='running', filter=('subtree', filter_comps))
            root_comp = reply_comp.data
            
            # 2. Fetch Terminal Device Properties
            print("Fetching terminal device properties...")
            filter_props = """
            <operational-mode-descriptors xmlns="http://openconfig.net/yang/openconfig-terminal-device-properties">
                <operational-modes/>
            </operational-mode-descriptors>
            <linecard-descriptors xmlns="http://openconfig.net/yang/openconfig-terminal-device-properties">
                <linecard-descriptor/>
            </linecard-descriptors>
            """
            # Note: This might still return two siblings. If it fails, we split again.
            # But usually top-level siblings in filter result in siblings in data.
            # Let's try wrapping them in a container if possible? No, we can't change server schema.
            # Let's try fetching them. If it fails, we will split further.
            
            try:
                # Must use get() because descriptors are operational state data
                reply_props = m.get(filter=('subtree', filter_props))
                root_props = reply_props.data
            except Exception as e:
                print(f"Warning: Batch fetch failed ({e}). Fetching descriptors individually...")
                # Fallback: Fetch separately
                f1 = """<operational-mode-descriptors xmlns="http://openconfig.net/yang/openconfig-terminal-device-properties"/>"""
                r1 = m.get(filter=('subtree', f1))
                root_props_modes = r1.data
                
                f2 = """<linecard-descriptors xmlns="http://openconfig.net/yang/openconfig-terminal-device-properties"/>"""
                r2 = m.get(filter=('subtree', f2))
                root_props_lines = r2.data
                
                # We will handle searching in both
                root_props = [root_props_modes, root_props_lines] 

            # Helper to search in one or list of roots
            def find_in_roots(roots, xpath, ns):
                if isinstance(roots, list):
                    for r in roots:
                        res = r.findall(xpath, ns)
                        if res: return res
                    return []
                else:
                    return roots.findall(xpath, ns)
            
            
            # Namespaces map for XPath queries
            ns = {
                'oc-platform': 'http://openconfig.net/yang/platform',
                'oc-opt-types': 'http://openconfig.net/yang/transport-types',
                'oc-term-props': 'http://openconfig.net/yang/openconfig-terminal-device-properties',
                'oc-term': 'http://openconfig.net/yang/terminal-device', 
                'oc-transceiver': 'http://openconfig.net/yang/platform/transceiver'
            }

            print("\n--- Ports & Transport Types ---")
            
            components = root_comp.findall('.//oc-platform:component', ns)
            
            for comp in components:
                name = comp.find('oc-platform:name', ns).text
                
                # property scan
                properties = comp.findall('.//oc-platform:property', ns)
                port_type = "Unknown"
                
                for prop in properties:
                    p_name = prop.find('oc-platform:name', ns).text
                    if p_name == 'ODTN-PORT-TYPE':
                        val = prop.find('.//oc-platform:config/oc-platform:value', ns)
                        if val is None:
                             val = prop.find('.//oc-platform:state/oc-platform:value', ns)
                        if val is not None:
                            port_type = val.text
                
                if port_type in ['CLIENT', 'LINE']:
                    print(f"\n{name}:")
                    print(f"  Transport Type: {port_type}")
                    
                    if port_type == 'LINE':
                        found_och = False
                        for och_comp in components:
                            och_config = och_comp.find('.//oc-term:optical-channel/oc-term:config', ns)
                            if och_config is not None:
                                assigned_port = och_config.find('oc-term:line-port', ns)
                                if assigned_port is not None and assigned_port.text == name:
                                    found_och = True
                                    mode = och_config.find('oc-term:operational-mode', ns).text
                                    print(f"  Operational Mode: {mode}")
                                    
                                    # We search broadly for the constrained-compatible-mode with this ID
                                    if 'root_props_modes' in locals():
                                         # used fallback
                                         constraints = find_in_roots(root_props, f'.//oc-term-props:constrained-compatible-mode', ns)
                                    else:
                                         constraints = root_props.findall(f'.//oc-term-props:constrained-compatible-mode', ns)
                                    
                                    # print(f"  DEBUG: Found {len(constraints)} constraint entries to search.")

                                    for constraint in constraints:
                                        c_mode_id = constraint.find('oc-term-props:mode-id', ns)
                                        # print(f"  DEBUG: Checking constraint with mode-id: {c_mode_id.text if c_mode_id is not None else 'None'}")
                                        if c_mode_id is not None and c_mode_id.text == mode:
                                            state_vals = constraint.find('.//oc-term-props:optical-channel-config-value-constraints/oc-term-props:state', ns)
                                            if state_vals is not None:
                                                min_f = state_vals.find('oc-term-props:min-central-frequency', ns)
                                                max_f = state_vals.find('oc-term-props:max-central-frequency', ns)
                                                
                                                if min_f is not None and max_f is not None:
                                                    print(f"  Min Frequency: {min_f.text} MHz")
                                                    print(f"  Max Frequency: {max_f.text} MHz")
                                                else:
                                                    print("  Constraints found but frequency limits missing.")
                                            # else:
                                                 # print("  DEBUG: found mode match but no state_vals")
                                            break
                                    break
                        if not found_och:
                             print("  No associated Optical Channel found.")
            
            # Print Operational Modes Catalog
            print("\n--- Supported Operational Modes Catalog (Manifest) ---")
            modes = find_in_roots(root_props, './/oc-term-props:operational-modes', ns) if 'root_props_modes' in locals() else root_props.findall('.//oc-term-props:operational-modes', ns)
            
            for m in modes:
                m_id = m.find('.//oc-term-props:state/oc-term-props:mode-id', ns)
                if m_id is None:
                    m_id = m.find('oc-term-props:mode-id', ns)
                m_id_str = m_id.text if m_id is not None else "?"
                
                bit_rate = m.find('.//oc-term-props:state/oc-term-props:bit-rate', ns)
                br_str = bit_rate.text.split(':')[-1] if bit_rate is not None and bit_rate.text else "?"
                
                baud_rate = m.find('.//oc-term-props:state/oc-term-props:baud-rate', ns)
                if baud_rate is not None and baud_rate.text:
                    try:
                        bd_val = float(baud_rate.text) / 1e9
                        bd_str = f"{bd_val:.2f} GBd"
                    except:
                        bd_str = baud_rate.text
                else:
                    bd_str = "?"
                
                mod_fmt = m.find('.//oc-term-props:state/oc-term-props:modulation-format', ns)
                mod_str = mod_fmt.text.split(':')[-1].replace("MODULATION_FORMAT_", "") if mod_fmt is not None and mod_fmt.text else "?"
                
                fec = m.find('.//oc-term-props:fec/oc-term-props:state/oc-term-props:fec-coding', ns)
                fec_str = fec.text.split(':')[-1] if fec is not None and fec.text else "?"
                
                width = m.find('.//oc-term-props:state/oc-term-props:optical-channel-spectrum-width', ns)
                w_str = f"{width.text} GHz" if width is not None and width.text else "?"
                
                print(f"Mode {m_id_str:>2}: BitRate={br_str:<14} Baud={bd_str:<12} Mod={mod_str:<10} FEC={fec_str:<8} Width={w_str}")

    except Exception as e:
        print(f"Error: {e}")
        # print(e.xml) # Uncomment to see raw XML on error if supported

if __name__ == '__main__':
    get_ports_and_modes()
