import socket
import platform
import subprocess

def check_mdns(hostname: str):
    try:
        print(f"🔍 Tentando resolver {hostname} ...")
        ip = socket.gethostbyname(hostname)
        print(f"✅ Resolvido com sucesso: {hostname} → {ip}")
        return ip
    except socket.gaierror as e:
        print(f"❌ Não foi possível resolver {hostname}: {e}")
        return None

def ping(ip: str):
    system = platform.system().lower()
    cmd = ["ping", "-n", "1", ip] if system == "windows" else ["ping", "-c", "1", ip]

    try:
        print(f"📡 Testando conectividade com {ip} ...")
        subprocess.check_output(cmd, stderr=subprocess.STDOUT)
        print("✅ Dispositivo respondeu ao ping.")
        return True
    except subprocess.CalledProcessError:
        print("⚠️ O dispositivo não respondeu ao ping.")
        return False

if __name__ == "__main__":
    # 🟩 coloque o nome mDNS da sua ESP32 aqui:
    hostname = "iikit5.local"

    ip = check_mdns(hostname)
    if ip:
        ping(ip)
    else:
        print("⚠️ mDNS não está respondendo. Verifique se a ESP32 está na mesma rede e com mDNS habilitado.")
