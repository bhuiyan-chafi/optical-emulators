import paramiko
import time
import sys

def test_ssh_cli():
    host = "localhost"
    port = 2025  # The CLI port we just mapped
    username = "admin"
    password = "admin"

    print(f"Connecting to {host}:{port} as {username}...")
    
    try:
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        
        # ConfD (Erlang SSH) might need legacy algorithms
        # We can try to allow all supported algorithms by not restricting them,
        # but sometimes we need to explicitly enable them if they are disabled by default in newer Paramiko.
        # Let's try connecting with default first, but if it fails, we might need a Transport options tweak.
        # Actually, "No existing session" often means auth failure or immediate disconnect.
        # Let's add look_for_keys=False to avoid local agent interference.
        
        client.connect(
            host, 
            port=port, 
            username=username, 
            password=password, 
            timeout=10, 
            look_for_keys=False, 
            allow_agent=False,
            # Enable legacy algorithms just in case
            disabled_algorithms=None 
        )

        
        # Invoke a shell
        channel = client.invoke_shell()
        
        # Wait for banner/prompt
        timeout = 5
        start = time.time()
        output = b""
        
        while time.time() - start < timeout:
            if channel.recv_ready():
                data = channel.recv(1024)
                output += data
                # ConfD usually sends a prompt like 'admin@confd>' or just '#'
                # Standard greeting often includes "Welcome to ConfD"
                # Let's check for '>' which is standard in Cisco-style CLI
                if b">" in output or b"#" in output:
                    print("SUCCESS: Received prompt/output from ConfD CLI:")
                    print("-" * 20)
                    print(output.decode('utf-8', errors='ignore'))
                    print("-" * 20)
                    return True
            time.sleep(0.5)
            
        print("TIMEOUT: No prompt received.")
        print("Partial Output:", output.decode('utf-8', errors='ignore'))
        return False
        
    except Exception as e:
        print(f"FAILED: Connection error: {e}")
        return False
    finally:
        client.close()

if __name__ == "__main__":
    if test_ssh_cli():
        sys.exit(0)
    else:
        sys.exit(1)
