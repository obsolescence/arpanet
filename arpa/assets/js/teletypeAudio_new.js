import embeddedAudio from './embedded_audio.js';

/**
 * Teletype Audio Manager (Web Audio API Version)
 * Handles terminal sounds with high precision and efficiency.
 * - Motor/key sounds: Still uses standard <audio> elements as they are simple, long-running sounds.
 * - Printing sounds: Uses the Web Audio API to play short, randomized snippets from audio buffers.
 *   This is highly efficient, low-latency, and sounds more natural.
 *
 * NOTE: The Web Audio API's fetch() method requires a web server environment (http:// or https://)
 * to avoid CORS errors. This will not work reliably from a local file:// URL.
 */

class TeletypeAudio {
  constructor() {
    this.soundEnabled = false;
    this.motorRunning = false;

    // Web Audio API properties
    this.audioContext = null;
    this.printCharBuffer = null;
    this.printSpaceBuffer = null;
    this.isInitialized = false;
  }

  /**
   * Initializes the AudioContext and loads the printing sound samples into AudioBuffers.
   * This must be called after a user interaction (e.g., a button click).
   */
  async init() {
    if (this.isInitialized) return;

    try {
      this.audioContext = new (window.AudioContext || window.webkitAudioContext)();

      // Helper function to decode Base64 data URI
      const decodeAudioDataFromURI = async (dataURI) => {
        const base64 = dataURI.split(',')[1];
        const binary = atob(base64);
        const len = binary.length;
        const bytes = new Uint8Array(len);
        for (let i = 0; i < len; i++) {
          bytes[i] = binary.charCodeAt(i);
        }
        return this.audioContext.decodeAudioData(bytes.buffer);
      };

      [this.printCharBuffer, this.printSpaceBuffer] = await Promise.all([
        decodeAudioDataFromURI(embeddedAudio['down-print-chars-01']),
        decodeAudioDataFromURI(embeddedAudio['down-print-spaces-01'])
      ]);

      this.isInitialized = true;
      this.soundEnabled = true;
      console.log('TeletypeAudio (Web Audio) initialized successfully from embedded data.');
    } catch (e) {
      console.error('Failed to initialize Web Audio for teletype:', e);
      // Fallback or disable audio features if initialization fails
      this.isInitialized = false;
      this.soundEnabled = false;
    }
  }

  _playSoundSnippet(buffer) {
    if (!this.soundEnabled || !buffer || !this.audioContext) return;

    const source = this.audioContext.createBufferSource();
    source.buffer = buffer;

    // Play a 100ms snippet from a random position
    const offset = Math.random() * (buffer.duration - 0.1);
    const duration = 0.1;

    source.connect(this.audioContext.destination);
    source.start(0, offset, duration);
  }

  /**
   * Start terminal motor sounds: motor-on once, then hum loop
   */
  startMotor() {
    if (this.motorRunning) return;

    const motorOnAudio = document.getElementById('motorOnSound');
    const motorHumAudio = document.getElementById('motorHumSound');

    if (!motorOnAudio || !motorHumAudio) return;

    this.motorRunning = true;
    this.soundEnabled = true;

    motorOnAudio.currentTime = 0;
    motorOnAudio.play().catch(err => console.error('Motor on sound failed:', err));

    motorOnAudio.onended = () => {
      if (this.motorRunning) {
        motorHumAudio.loop = true;
        motorHumAudio.currentTime = 0;
        motorHumAudio.play().catch(err => console.error('Motor hum sound failed:', err));
      }
    };
  }

  /**
   * Stop terminal motor and play motor-off sound
   */
  stopMotor() {
    if (!this.motorRunning) return;

    const motorOnAudio = document.getElementById('motorOnSound');
    const motorHumAudio = document.getElementById('motorHumSound');
    const motorOffAudio = document.getElementById('motorOffSound');
    this.motorRunning = false;

    if (!motorOnAudio || !motorHumAudio || !motorOffAudio) return;

    motorOnAudio.pause();
    motorHumAudio.pause();

    motorOffAudio.currentTime = 0;
    motorOffAudio.play().catch(err => console.error('Motor off sound failed:', err));
  }

  /**
   * Toggle sound on/off
   */
  toggle() {
    const motorHumAudio = document.getElementById('motorHumSound');
    if (!motorHumAudio) return this.soundEnabled;

    this.soundEnabled = !this.soundEnabled;

    if (this.soundEnabled) {
      if (this.motorRunning) {
        motorHumAudio.play().catch(err => console.log('Audio playback failed:', err));
      }
    } else {
      motorHumAudio.pause();
    }
    console.log(`Sound enabled: ${this.soundEnabled}`);
    return this.soundEnabled;
  }

  /**
   * Play random key down sound
   */
  playKeyDown() {
    if (!this.soundEnabled) return;

    const randomIndex = Math.floor(Math.random() * 7) + 1;
    const audio = document.getElementById(`downKey${randomIndex}`);
    if (audio) {
      audio.currentTime = 0;
      audio.play().catch(err => console.log('Key down sound failed:', err));
    }
  }

  /**
   * Play a snippet of the character printing sound.
   */
  playPrintChar() {
    this._playSoundSnippet(this.printCharBuffer);
  }

  /**
   * Play a snippet of the space character printing sound.
   */
  playPrintSpace() {
    this._playSoundSnippet(this.printSpaceBuffer);
  }

  isEnabled() {
    return this.soundEnabled;
  }
}

// Create global instance
const teletypeAudio = new TeletypeAudio();
