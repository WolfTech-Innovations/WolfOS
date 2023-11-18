import os
from discord import Webhook, RequestsWebhookAdapter, Embed, Color
from dotenv import load_dotenv

load_dotenv()

# Discord Webhook Setup
discord_webhook_url = os.getenv('https://discord.com/api/webhooks/1175551243414949998/PRGTbBzRiIICLvtLduuhnhIxhmjI34F2n7eY3a9IdYQik5vN44HXzzb-PcgJkGSPMCE-')
discord_webhook = Webhook.from_url(discord_webhook_url, adapter=RequestsWebhookAdapter())

# Example Discord notification
embed = Embed(
    title='GitHub Action Triggered!',
    description='There has been a new push to the repository.',
    color=Color.green()
)

discord_webhook.send(embed=embed)
