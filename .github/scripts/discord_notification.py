import os
from discord import Webhook, RequestsWebhookAdapter, Embed, Color

# Discord Webhook Setup using Bot Token
discord_bot_token = os.getenv('MTE3NTUyMDcwNjA5NjgxMjE4Mw.Grgl7j.ywQJ5EEDiezRqsv0veLPdphT8129yrqUv4BlR8')
discord_webhook = Webhook.from_bot_token(discord_bot_token, adapter=RequestsWebhookAdapter())

# Example Discord notification
embed = Embed(
    title='GitHub Action Triggered!',
    description='There has been a new push to the repository.',
    color=Color.green()
)

discord_webhook.send(embed=embed)
