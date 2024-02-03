import os
import requests
from dotenv import load_dotenv

load_dotenv()

def send_discord_notification(message):
    discord_webhook_url = os.getenv('DISCORD_WEBHOOK_URL')

    if discord_webhook_url is None:
        raise ValueError('Discord webhook URL is missing. Make sure to set DISCORD_WEBHOOK_URL in your .env file.')

    payload = {
        'content': message,
        'embeds': [
            {
                'title': 'Build step complete! Waiting on next step to complete . . .'
                'color': 65280
            }
        ]
    }

    headers = {
        'Content-Type': 'application/json'
    }

    response = requests.post(discord_webhook_url, json=payload, headers=headers)

    if response.status_code == 200:
        print('Notification sent successfully!')
    else:
        print(f'Error sending notification. Status code: {response.status_code}, Response: {response.text}')

if __name__ == "__main__":
    # Example Discord notification
    send_discord_notification('GitHub Action Triggered!')
