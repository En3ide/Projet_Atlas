<template>
  <div ref="terminalRef" class="ssh-terminal"></div>
</template>

<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref } from 'vue'
import { Terminal } from '@xterm/xterm'
import { FitAddon } from '@xterm/addon-fit'

import '@xterm/xterm/css/xterm.css'

const terminalRef = ref<HTMLDivElement | null>(null)

let term: Terminal | null = null
let socket: WebSocket | null = null
let fitAddon: FitAddon | null = null

let currentLine = ''

onMounted(() => {
  if (!terminalRef.value) return

  term = new Terminal({
    cursorBlink: true,
    fontSize: 15,
    fontFamily: 'monospace',
    theme: {
      background: '#0a0a0a',
      foreground: '#ffffff',
      cursor: '#00ff9c',
      black: '#000000',
      red: '#ff5f56',
      green: '#27c93f',
      yellow: '#ffbd2e',
      blue: '#4da3ff',
      magenta: '#c678dd',
      cyan: '#00ffff',
      white: '#ffffff'
    }
  })

  fitAddon = new FitAddon()
  term.loadAddon(fitAddon)

  term.open(terminalRef.value)
  fitAddon.fit()

  term.writeln('Atlas Terminal')
  term.writeln('Mode simulation actif')
  term.writeln('')
  term.writeln('Tape "help" pour voir les commandes disponibles.')
  term.writeln('')
  term.write('atlas@server:~$ ')

  /*
    Pour brancher le terminal au serveur SSH, on décommente ce bloc
    et on désactive le bloc de simulation juste en dessous.

    socket = new WebSocket('ws://localhost:3001')

    socket.onopen = () => {
      term?.writeln('WebSocket connecté.')
    }

    socket.onmessage = (event) => {
      term?.write(event.data)
    }

    socket.onerror = () => {
      term?.writeln('\r\nErreur WebSocket.')
    }

    socket.onclose = () => {
      term?.writeln('\r\nConnexion fermée.')
    }

    term.onData((data) => {
      socket?.send(data)
    })
  */

  // Pour l'instant, on simule les commandes du terminal

  term.onData((data) => {
    if (!term) return

    // Entrée : on lance la commande et on passe à la ligne suivante
    if (data === '\r') {
      term.write('\r\n')
      handleCommand(currentLine)
      currentLine = ''
      term.write('\r\natlas@server:~$ ')
      return
    }

    // Retour arrière : on efface le dernier caractère
    if (data === '\u007F') {
      if (currentLine.length > 0) {
        currentLine = currentLine.slice(0, -1)
        term.write('\b \b')
      }

      return
    }

    currentLine += data
    term.write(data)
  })

  window.addEventListener('resize', resizeTerminal)
})

onBeforeUnmount(() => {
  window.removeEventListener('resize', resizeTerminal)

  ;(socket as WebSocket | null)?.close()
  term?.dispose()
})

function resizeTerminal() {
  fitAddon?.fit()
}

function handleCommand(cmd: string) {
  if (!term) return

  const command = cmd.trim()

  switch (command) {
    case '':
      break

    case 'help':
      term.writeln('Commandes disponibles :')
      term.writeln('  help       Affiche les commandes')
      term.writeln('  status     Affiche l’état du serveur')
      term.writeln('  devices    Liste les appareils réseau')
      term.writeln('  storage    Affiche l’état du stockage')
      term.writeln('  neofetch   Affiche les infos Atlas')
      term.writeln('  clear      Nettoie le terminal')
      break

    case 'status':
      term.writeln('System Status : ONLINE')
      term.writeln('CPU Usage     : 24%')
      term.writeln('RAM Usage     : 8.2 GB / 32 GB')
      term.writeln('Network       : OK')
      term.writeln('Uptime        : 12 days')
      break

    case 'devices':
      term.writeln('192.168.1.10  desktop-main')
      term.writeln('192.168.1.12  laptop')
      term.writeln('192.168.1.20  nas-server')
      term.writeln('192.168.1.30  camera-entry')
      term.writeln('192.168.1.31  camera-garage')
      break

    case 'storage':
      term.writeln('/dev/sda1   420GB / 1TB')
      term.writeln('/dev/sdb1   1.8TB / 4TB')
      term.writeln('/backup     860GB / 2TB')
      break

    case 'neofetch':
      term.writeln('      ___   _   _             ')
      term.writeln('     / _ \\ | | | |            ')
      term.writeln('    / /_\\ \\| |_| | __ _ ___   ')
      term.writeln('    |  _  || __| |/ _` / __|  ')
      term.writeln('    | | | || |_| | (_| \\__ \\  ')
      term.writeln('    \\_| |_/ \\__|_|\\__,_|___/  ')
      term.writeln('')
      term.writeln('AtlasOS 1.0')
      term.writeln('Host   : atlas-server')
      term.writeln('Kernel : Linux')
      term.writeln('Shell  : simulated')
      term.writeln('Status : ONLINE')
      break

    case 'clear':
      term.clear()
      break

    default:
      term.writeln(`command not found: ${command}`)
      break
  }
}
</script>

<style scoped>
.ssh-terminal {
  width: 100%;
  max-width: 100%;
  height: 75vh;
  min-height: 500px;

  background-color: #0a0a0a;

  border-radius: 18px;
  overflow: hidden;

  padding: 12px;
  box-sizing: border-box;
}
</style>
