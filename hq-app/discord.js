// Discord-Anbindung für das Overlay: wer ist online, wer sitzt in welchem Voice-Channel,
// letzte Nachrichten lesen, Nachrichten und Updates posten.
const { Client, GatewayIntentBits, ChannelType, Events } = require('discord.js');

let client = null;
let onChange = () => {};
let timer = null;

function bump() { clearTimeout(timer); timer = setTimeout(() => onChange(snapshot()), 400); }

function snapshot(guildId) {
  if (!client || !client.isReady()) return { ready: false };
  const g = guildId ? client.guilds.cache.get(guildId) : client.guilds.cache.first();
  if (!g) return { ready: true, guilds: client.guilds.cache.map(x => ({ id: x.id, name: x.name })), noGuild: true };
  const rank = { online: 0, idle: 1, dnd: 2 };
  const members = g.members.cache.filter(m => !m.user.bot).map(m => ({
    id: m.id, name: m.displayName, avatar: m.displayAvatarURL({ size: 64, extension: 'png' }),
    status: m.presence?.status || 'offline', game: m.presence?.activities?.find(a => a.type === 0)?.name || '',
    voice: m.voice?.channelId || ''
  })).sort((a, b) => ((rank[a.status] ?? 3) - (rank[b.status] ?? 3)) || a.name.localeCompare(b.name));
  const me = client.user;
  const text = g.channels.cache.filter(c => c.type === ChannelType.GuildText && c.viewable)
    .sort((a, b) => a.rawPosition - b.rawPosition).map(c => ({ id: c.id, name: c.name, topic: c.topic || '', canSend: c.permissionsFor(me)?.has('SendMessages') || false }));
  const voice = g.channels.cache.filter(c => c.type === ChannelType.GuildVoice || c.type === ChannelType.GuildStageVoice)
    .sort((a, b) => a.rawPosition - b.rawPosition).map(c => ({ id: c.id, name: c.name, members: c.members.filter(m => !m.user.bot).map(m => m.displayName) }));
  return { ready: true, bot: me.tag, guild: { id: g.id, name: g.name, icon: g.iconURL({ size: 64, extension: 'png' }) || '' },
    guilds: client.guilds.cache.map(x => ({ id: x.id, name: x.name })), members, text, voice };
}

async function connect(token, notify) {
  await disconnect();
  onChange = notify;
  client = new Client({ intents: [
    GatewayIntentBits.Guilds, GatewayIntentBits.GuildMembers, GatewayIntentBits.GuildPresences,
    GatewayIntentBits.GuildVoiceStates, GatewayIntentBits.GuildMessages, GatewayIntentBits.MessageContent
  ] });
  client.on(Events.PresenceUpdate, bump);
  client.on(Events.VoiceStateUpdate, bump);
  client.on(Events.GuildMemberAdd, bump);
  client.on(Events.GuildMemberRemove, bump);
  client.on(Events.ChannelCreate, bump);
  client.on(Events.ChannelDelete, bump);
  client.on(Events.MessageCreate, m => onChange({ ...snapshot(m.guildId), newMessage: { channelId: m.channelId } }));
  await client.login(token);
  await new Promise(res => client.isReady() ? res() : client.once(Events.ClientReady, res));
  return snapshot();
}
async function disconnect() { if (client) { try { await client.destroy(); } catch {} client = null; } }

function describeError(e) {
  const m = String(e && e.message || e);
  if (/disallowed intents/i.test(m)) return 'Im Developer-Portal beim Bot die drei Privileged Intents einschalten (Presence, Server Members, Message Content).';
  if (/invalid token|TokenInvalid|An invalid token/i.test(m)) return 'Der Token stimmt nicht. Im Developer-Portal unter Bot einen neuen Token erzeugen.';
  if (/Missing Access|Missing Permissions/i.test(m)) return 'Dem Bot fehlen Rechte in diesem Channel.';
  return m;
}

async function messages(channelId, limit = 25) {
  const ch = await client.channels.fetch(channelId);
  const list = await ch.messages.fetch({ limit });
  return [...list.values()].reverse().map(m => ({
    id: m.id, author: m.member?.displayName || m.author.username, bot: m.author.bot,
    text: m.content || (m.attachments.size ? '[Datei: ' + [...m.attachments.values()].map(a => a.name).join(', ') + ']' : ''),
    at: m.createdTimestamp
  }));
}
async function send(channelId, text) {
  const ch = await client.channels.fetch(channelId);
  const t = String(text || '').trim().slice(0, 1900);
  if (!t) throw new Error('Leere Nachricht');
  await ch.send({ content: t, allowedMentions: { parse: [] } });
}

module.exports = { connect, disconnect, snapshot, messages, send, describeError };
