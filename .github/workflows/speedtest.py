import os
import subprocess
import time

def ping_internet_archive():
    # Ping the Internet Archive
    ping_result = subprocess.run(["ping", "-c", "4", "archive.org"], capture_output=True, text=True)
    if ping_result.returncode == 0:
        print("Ping to Internet Archive successful!")
    else:
        print("Failed to ping Internet Archive. Check your internet connection.")
        exit(1)

def check_download_speed():
    # Check download speed using speedtest-cli
    speedtest_result = subprocess.run(["speedtest-cli", "--simple"], capture_output=True, text=True)
    if speedtest_result.returncode == 0:
        lines = speedtest_result.stdout.split('\n')
        download_speed = int(lines[1].split()[1])
        print(f"Download Speed: {download_speed} Mbps")
        return download_speed
    else:
        print("Failed to check download speed. Continuing with workflow.")
        return 0

def main():
    ping_internet_archive()
    min_download_speed = 50  # Adjust as needed (in Mbps)
    while True:
        download_speed = check_download_speed()
        if download_speed >= min_download_speed:
            print("High download speed detected. Continuing with workflow.")
            break
        else:
            print("Waiting for high download speed...")
            time.sleep(60)  # Wait for 1 minute before checking again

if __name__ == "__main__":
    main()
