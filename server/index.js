const enet = require('enet');
const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const PORT = 3000;
const HOST = '0.0.0.0';
const LOG_FILE = path.join(__dirname, 'server.log');
const PID_FILE = path.join(__dirname, 'server.pid');

// Write PID for easier management
fs.writeFileSync(PID_FILE, process.pid.toString());

function logger(msg, level = 'INFO') {
    const timestamp = new Date().toISOString();
    const line = `[${timestamp}] [${level}] ${msg}`;
    console.log(line);
    try {
        fs.appendFileSync(LOG_FILE, line + '\n');
    } catch (e) {
        console.error("Failed to write to log file", e);
    }
}

function errorLogger(msg, err) {
    const timestamp = new Date().toISOString();
    const stack = err ? `\nStack: ${err.stack}` : '';
    const line = `[${timestamp}] [ERROR] ${msg}${stack}`;
    console.error(line);
    try {
        fs.appendFileSync(LOG_FILE, line + '\n');
    } catch (e) {
        console.error("Failed to write to log file", e);
    }
}

// Clear log on startup
try {
    fs.writeFileSync(LOG_FILE, `--- Server Started: ${new Date().toISOString()} (PID: ${process.pid}) ---\n`);
} catch (e) {
    console.error("Could not initialize log file", e);
}

// Global error handlers
process.on('uncaughtException', (err) => {
    errorLogger('Uncaught Exception', err);
});

process.on('unhandledRejection', (reason, promise) => {
    errorLogger('Unhandled Rejection', reason);
});

const clients = new Map();

function startServer() {
    enet.createServer({
        address: { address: HOST, port: PORT },
        peers: 32,
        channels: 2,
        down: 0,
        up: 0
    }, function(err, server) {
        if (err) {
            if (err.code === 'EADDRINUSE') {
                logger(`Port ${PORT} is in use. Attempting to resolve...`, 'WARN');
                try {
                    // Try to kill the process on that port (Windows specific)
                    const output = execSync(`netstat -ano | findstr :${PORT}`).toString();
                    const lines = output.split('\n');
                    for (const line of lines) {
                        if (line.includes('UDP')) {
                            const parts = line.trim().split(/\s+/);
                            const pid = parts[parts.length - 1];
                            if (pid && pid !== process.pid.toString()) {
                                logger(`Killing existing server process with PID ${pid}...`);
                                execSync(`taskkill /F /PID ${pid}`);
                            }
                        }
                    }
                    // Wait a bit and try again
                    setTimeout(startServer, 1000);
                    return;
                } catch (e) {
                    errorLogger("Failed to auto-resolve EADDRINUSE", e);
                }
            }
            errorLogger("Failed to create ENet server", err);
            process.exit(1);
        }

        logger(`Skyrim Multiplayer Server (ENet) listening on ${HOST}:${PORT}`);
        logger("Ready for connections. All incoming packets will be logged for debugging.");

        server.on("connect", function(peer, data) {
            const addr = peer.address();
            const ip = addr.address;
            const clientKey = `${ip}:${addr.port}`;
            
            // ONE-PLAYER-PER-IP RULE:
            // Check if there is already a client with this same IP.
            // If so, it's likely a reconnecting player whose old session is a 'ghost'.
            const normalizeIp = (ipStr) => ipStr.startsWith('::ffff:') ? ipStr.substring(7) : ipStr;
            const normIp = normalizeIp(ip);
            
            clients.forEach((c, key) => {
                const lastColon = key.lastIndexOf(':');
                const oldIp = normalizeIp(key.substring(0, lastColon));
                if (oldIp === normIp && key !== clientKey) {
                    logger(`[Connect] Kicking ghost session ${key} to allow new connection from ${clientKey}`, 'WARN');
                    c.peer.disconnectNow();
                    clients.delete(key);
                }
            });

            logger(`[Connect] Player joined from ${clientKey}`);

            clients.set(clientKey, {
                peer: peer,
                id: clientKey,
                x: 0, y: 0, z: 0, rot: 0,
                lastSeen: Date.now(),
                packetCount: 0
            });

            // Send welcome packet with ID
            const welcome = JSON.stringify({ type: "welcome", id: clientKey });
            const welcomePacket = new enet.Packet(Buffer.from(welcome), enet.PACKET_FLAG.RELIABLE);
            peer.send(0, welcomePacket);

            peer.on("message", function(packet, channel) {
                try {
                    const dataBuffer = packet.data();
                    const msgString = dataBuffer.toString().replace(/\0/g, ''); 
                    const parsed = JSON.parse(msgString);
                    const client = clients.get(clientKey);
                    
                    if (client) {
                        client.lastSeen = Date.now();
                        client.packetCount++;
                    }

                    // Detailed logging for position updates
                    if (parsed.type === "pos") {
                        if (client) {
                            client.x = parsed.x;
                            client.y = parsed.y;
                            client.z = parsed.z;
                            client.rot = parsed.rot;
                            client.cell = parsed.cell;
                            client.world = parsed.world;
                        }

                        // Sync notifications are now hidden (not logged to console) to reduce spam.

                    } else if (parsed.type === "spawn_log") {
                        logger(`[Client-Log] ${clientKey} reporting: ${parsed.msg}`, 'SUCCESS');
                    } else {
                        logger(`[Event] ${clientKey} sent ${parsed.type}: ${JSON.stringify(parsed)}`);
                    }

                    // Broadcast to others
                    parsed.id = clientKey;
                    const broadcastData = JSON.stringify(parsed);
                    // Use sequencing for animations and events, but unsequenced for position
                    const flags = (parsed.type === "pos") ? enet.PACKET_FLAG.UNSEQUENCED : enet.PACKET_FLAG.RELIABLE;
                    
                    const broadcastPacket = new enet.Packet(Buffer.from(broadcastData), flags);
                    
                    let broadcastCount = 0;
                    clients.forEach((c, key) => {
                        if (key !== clientKey) {
                            c.peer.send(channel, broadcastPacket);
                            broadcastCount++;
                        }
                    });

                    if (parsed.type !== "pos" && broadcastCount > 0) {
                        logger(`[Broadcast] Relayed ${parsed.type} to ${broadcastCount} other players`);
                    }

                } catch (e) {
                    errorLogger(`Failed to parse client message from ${clientKey}`, e);
                }
            });

            peer.on("disconnect", function() {
                logger(`[Disconnect] Player ${clientKey} left.`);
                clients.delete(clientKey);
            });
        });

        server.start(10);
    });
}

// Initial start
startServer();

// Add a simple error reader / diagnostic tool
setInterval(() => {
    const now = Date.now();
    clients.forEach((client, key) => {
        // 1. Connection Health
        if (now - client.lastSeen > 30000) { 
            logger(`[Diagnostic] Client ${key} inactive for 30s. Connection may be dropped.`, 'WARN');
        }

        // 2. Data Integrity Checks
        if (client.packetCount > 10) {
            if (client.x === 0 && client.y === 0 && client.z === 0) {
                logger(`[Diagnostic] Client ${key} reporting (0,0,0). Is the player loaded in a cell?`, 'WARN');
            }
            // If they are sending packets but the server hasn't seen a cell/world ID yet
            if (client.cell === 0 && client.world === 0) {
                // Note: cell 0 might be valid for some worldspaces, but usually not both 0
                logger(`[Diagnostic] Client ${key} reporting null cell/world. Sync might be broken.`, 'WARN');
            }
        }
    });

    if (clients.size === 0) {
        // Silent heartbeat
    } else {
        logger(`[Status] Active Players: ${clients.size}`);
    }
}, 15000);

