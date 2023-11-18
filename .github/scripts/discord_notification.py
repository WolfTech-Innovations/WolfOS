import os
import base64
import requests
from discord import Embed, Color

# Decode Base64-encoded token
base64_encoded_token = os.getenv('TVRFM05UVXlNRGN3TmpBNU5qZ3hNakU0TXcuR0tnb2FNLjNsTTNnRmIyWTZkQVBRa1hDQ2NmZU12TXYwY25JXzdENUVOc0Jn')
discord_bot_token = base64.b64decode(base64_encoded_token).decode('utf-8')

# Example Discord notification
embed = Embed(
    title='GitHub Action Triggered!',
    description='There has been a new push to the repository.',
    color=Color.green()
)

# Set up the Discord Webhook URL
discord_webhook_url = f'https://discord.com/api/webhooks/{discord_bot_token}'

# Send the webhook using the requests library
response = requests.post(discord_webhook_url, json={'embeds': [embed.to_dict()]})

# Check if the request was successful
if response.status_code == 204:
    print('Discord webhook sent successfully!')
else:
    print(f'Error sending Discord webhook. Status code: {response.status_code}')
    print(response.text)
