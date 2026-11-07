<template>
  <div class="contenu">

    <div class="pannel pannel-1">
      <h1>Atlas</h1>

      <p>
        Bienvenue sur <strong>Atlas</strong>, une plateforme centralisée de supervision et de gestion de votre infrastructure domestique et réseau local.
      </p>

      <p>
        Accédez à vos caméras, surveillez vos équipements, gérez votre NAS et supervisez votre réseau depuis une interface unique.
      </p>
    </div>

    <div class="grid-pannels video-pannel">

      <div
        v-for="camera in cameras"
        :key="camera.name"
        class="pannel camera-card"
      >

        <div class="camera-header">

          <h2>
            {{ camera.label }}
          </h2>

          <span class="camera-status">
            LIVE
          </span>

        </div>

        <video
          :ref="el => setVideoRef(el, camera.name)"
          class="camera-video"
          controls
          autoplay
          muted
          playsinline
        ></video>

      </div>

    </div>

  </div>
</template>

<script lang="ts">
import { defineComponent, nextTick } from 'vue'
import type { ComponentPublicInstance } from 'vue'
import Hls from 'hls.js'

type Camera = {
  name: string
  label: string
  url: string
}

export default defineComponent({

  name: 'Home',

  data() {
    return {

      videoRefs: {} as Record<string, HTMLVideoElement>,

      hlsInstances: {} as Record<string, Hls>,

      cameras: [

        {
          name: 'camera1',
          label: 'Amiens',
          url: 'https://visionenvironnement.quanteec.com/contents/encodings/live/6085bba1-eaf8-46e0-746c-7561-6665-64-a59e-d4220c8f0b8ed/media_0.m3u8'
        },

        {
          name: 'camera2',
          label: 'Webcam Bailleul - Boule Flamande',
          url: 'https://visionenvironnement.quanteec.com/contents/encodings/live/20b41781-b470-456b-746c-7561-6665-64-add0-68ce669556cbd/media_0.m3u8'
        },

        {
          name: 'camera3',
          label: 'Port de Nice',
          url: 'https://deliverys4.quanteec.com/contents/encodings/vod/2f7bbb49-b6c0-4bd1-3634-3130-6d61-63-8397-67c873cc0e13d/media_0.m3u8'
        },

        {
          name: 'camera4',
          label: 'Promenade des Anglais',
          url: 'https://ds2-cache.quanteec.com/contents/encodings/live/4c362a32-a803-482b-3338-3630-6d61-63-b2af-1318979a1b8fd/media_1.m3u8'
        },

        {
          name: 'camera5',
          label: 'Port de Nice (HLS)',
          url: 'https://deliverys4.quanteec.com/contents/encodings/vod/1875b0fa-4e81-4717-3634-3130-6d61-63-8d41-5d7e1a962a76d/media_1.m3u8'
        },
        {
          name: 'camera6',
          label: 'Port de Villefranche-sur-Mer (HLS)',
          url: 'https://ds2-cache.quanteec.com/contents/encodings/live/8c26eae2-6653-4f7d-3931-3730-6d61-63-a9d0-7259aeb98134d/master.m3u8'
        }
      ] as Camera[]
    }
  },

  mounted() {

    nextTick(() => {

      this.initVideos()
    })
  },

  beforeUnmount() {

    Object
      .values(this.hlsInstances)
      .forEach((hls) => {

        hls.destroy()
      })
  },

  methods: {

    setVideoRef(
  el: Element | ComponentPublicInstance | null,
  name: string
) {
  if (el instanceof HTMLVideoElement) {
    this.videoRefs[name] = el
  }
},

    initVideos() {

      this.cameras.forEach((camera) => {

        const video =
          this.videoRefs[camera.name]

        if (!video) return

        if (Hls.isSupported()) {

          const hls = new Hls()

          hls.loadSource(camera.url)

          hls.attachMedia(video)

          this.hlsInstances[camera.name] = hls

        } else if (
          video.canPlayType(
            'application/vnd.apple.mpegurl'
          )
        ) {

          video.src = camera.url
        }
      })
    }
  }
})
</script>

<style scoped>

/* Grille des caméras */

.video-pannel {
    display: grid;

    grid-template-columns:
      repeat(3, minmax(0, 1fr));

    gap: 24px;

    margin-top: 24px;

    width: 100%;
}

/* Panneau de chaque caméra */

.camera-card {
    padding: 0;

    overflow: hidden;

    min-height: 360px;
}

/* Nom de la caméra et état du flux */

.camera-header {
    height: 60px;

    padding: 0 20px;

    display: flex;

    align-items: center;
    justify-content: space-between;

    border-bottom:
      1px solid rgba(255,255,255,0.05);

    background-color:
      rgba(255,255,255,0.02);
}

.camera-header h2 {
    margin: 0;

    font-size: 18px;
}

/* Badge du flux vidéo */

.camera-status {
    color: rgb(0,255,120);

    background-color:
      rgba(0,255,120,0.12);

    border:
      1px solid rgba(0,255,120,0.2);

    padding: 6px 12px;

    border-radius: 10px;

    font-size: 12px;
    font-weight: bold;
}

/* Affichage de la vidéo */

.camera-video {
    width: 100%;

    height: 300px;

    display: block;

    background-color: black;

    object-fit: cover;
}

/* On adapte la grille à la taille de l'écran */

@media screen and (max-width: 1400px) {

    .video-pannel {

        grid-template-columns:
          repeat(2, 1fr);
    }
}

@media screen and (max-width: 768px) {

    .video-pannel {

        grid-template-columns: 1fr;
    }

    .camera-video {

        height: 240px;
    }
}

</style>
