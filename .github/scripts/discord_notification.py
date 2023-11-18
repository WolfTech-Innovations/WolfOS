import os
import base64
from discord import Webhook, RequestsWebhookAdapter, Embed, Color

# Decode Base64-encoded token
base64_encoded_token = os.getenv('TVRFM05UVXlNRGN3TmpBNU5qZ3hNakU0TXcuR0tnb2FNLjNsTTNnRmIyWTZkQVBRa1hDQ2NmZU12TXYwY25JXzdENUVOc0Jn')
discord_bot_token = base64.b64decode(base64_encoded_token).decode('utf-8')

# Discord Webhook Setup using Bot Token
discord_webhook = Webhook.from_bot_token(discord_bot_token, adapter=RequestsWebhookAdapter())

# Example Discord notification
embed = Embed(
    title='GitHub Action Triggered!',
    description='There has been a new push to the repository.',
    color=Color.green()
)

discord_webhook.send(embed=embed)
