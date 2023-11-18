import os
import base64
import requests
from discord import Embed, Color

try:
    # Get the Base64-encoded token from the environment variable
    base64_encoded_token = os.getenv('TVRFM05UVXlNRGN3TmpBNU5qZ3hNakU0TXcuR0tnb2FNLjNsTTNnRmIyWTZkQVBRa1hDQ2NmZU12TXYwY25JXzdENUVOc0Jn')

    # Check if the token is not empty
    if base64_encoded_token:
        # Decode Base64-encoded token
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
        response.raise_for_status()
        print('Discord webhook sent successfully!')
    else:
        print('BASE64_ENCODED_TOKEN environment variable is not set. Please set the variable with your Base64-encoded token.')
except Exception as e:
    print(f'An error occurred: {e}')
