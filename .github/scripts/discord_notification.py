import os
import requests
from dotenv import load_dotenv

load_dotenv()

# Discord Webhook Setup
discord_webhook_url = os.getenv('https://discord.com/api/webhooks/1175551243414949998/PRGTbBzRiIICLvtLduuhnhIxhmjI34F2n7eY3a9IdYQik5vN44HXzzb-PcgJkGSPMCE-')

if discord_webhook_url is None:
    raise ValueError('Discord webhook URL is missing. Make sure to set DISCORD_WEBHOOK_URL in your .env file.')

# Example Discord notification
payload = {
    'content': 'GitHub Action Triggered!',
    'embeds': [
        {
            'title': 'There has been a new push to the repository.',
            'color': 65280  # Green color
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
