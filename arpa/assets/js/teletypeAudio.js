/**
 * Teletype Audio Manager
 * Handles terminal sounds: motor, key presses, and character printing
 *
 * CRITICAL - PRINTING SOUND IMPLEMENTATION:
 * - Motor/key sounds: HTML <audio> elements (play full file)
 * - Printing sounds: HTML <audio> elements + setTimeout
 * - Method: Set currentTime to position, play(), pause after 100ms with setTimeout
 * - Position cycles: 0, 0.1, 0.2, ... 4.5, back to 0 (for variety)
 * - 100ms duration synchronized with character output
 * - Works from file:// (no CORS issues)
 * - DO NOT use Web Audio API fetch/XMLHttpRequest (blocked by CORS on file://)
 * - See claude.md for full documentation
 *
 * CROSSFADE FIX FOR MOTOR HUM (Chromium gap issue):
 * - Uses two motor hum audio elements that alternate for gapless looping
 * - Fixes brief gap in Chromium when looping motor hum sample
 * - Firefox works fine with single element, but Chromium needs crossfade
 *
 * TO ROLLBACK THE CROSSFADE FIX:
 * 1. In arpanet_terminal2.html: Remove <audio id="motorHumSound2"> element
 * 2. In this file: Search for "CROSSFADE FIX" comments and follow rollback instructions
 * 3. Modified sections: constructor, startMotor(), stopMotor(), toggle()
 * 4. Delete method: startMotorCrossfade()
 * All changes are marked with "CROSSFADE FIX" / "TO ROLLBACK" comments
 */

class TeletypeAudio {
  constructor() {
    this.soundEnabled = false;
    this.motorRunning = false;
    this.printCharSoundPosition = 0; // Position in buffer (0-4.5s, cycles)
    this.printSpaceSoundPosition = 0; // Position in buffer (0-4.5s, cycles)

    // Fragment cycling (0-6)
    this.currentFragment = 0;
    this.numFragments = 7;

    // CROSSFADE FIX: Track alternating motor hum elements for gapless looping
    // TO ROLLBACK: Remove these three lines
    this.currentMotorHum = 1; // 1 or 2 - which element is currently primary
    this.motorCrossfadeTimer = null; // Timer for checking when to crossfade
    // END CROSSFADE FIX
  }

  /**
   * Initialize audio - verifies fragment samples are loaded
   * Avoids CORS issues with fetch() on file:// URLs
   */
  init() {
    // Verify fragment samples are loaded
    const testFragment = document.getElementById('printChar00');
    if (testFragment && testFragment.readyState >= 2) {
      console.log('✓ Print fragment samples loaded');
    } else {
      console.error('✗ Print fragment samples NOT loaded');
    }

    this.soundEnabled = false;
  }

  /**
   * Start terminal motor sounds: motor-on once, then hum loop
   * CROSSFADE FIX: Uses two alternating motor hum elements for gapless looping in Chromium
   * TO ROLLBACK: Replace this entire function with the original simple loop version
   */
  startMotor() {
    if (this.motorRunning) return;

    const motorOnAudio = document.getElementById('motorOnSound');
    const motorHumAudio = document.getElementById('motorHumSound');
    const motorHumAudio2 = document.getElementById('motorHumSound2');

    if (!motorOnAudio || !motorHumAudio || !motorHumAudio2) return;

    this.motorRunning = true;
    this.soundEnabled = true;

    // Play motor-on sound
    motorOnAudio.currentTime = 0;
    motorOnAudio.play().catch(err => console.log('Audio playback failed:', err));

    // When motor-on finishes, start crossfading motor hum loop
    motorOnAudio.onended = () => {
      // CROSSFADE FIX: Start alternating crossfade loop instead of simple loop
      // TO ROLLBACK: Replace the lines below with original simple loop code:
      //   motorHumAudio.loop = true;
      //   motorHumAudio.currentTime = 0;
      //   motorHumAudio.play().catch(err => console.log('Audio playback failed:', err));

      // Disable native loop - we'll handle looping manually via crossfade
      motorHumAudio.loop = false;
      motorHumAudio2.loop = false;

      // Start first motor hum
      this.currentMotorHum = 1;
      motorHumAudio.currentTime = 0;
      motorHumAudio.play().catch(err => console.log('Motor hum playback failed:', err));

      // Set up crossfade monitoring
      this.startMotorCrossfade(motorHumAudio, motorHumAudio2);
      // END CROSSFADE FIX
    };
  }

  /**
   * Stop terminal motor and play motor-off sound
   * CROSSFADE FIX: Also stops second motor hum element and clears crossfade timer
   * TO ROLLBACK: Remove motorHumAudio2 references and timer clearing
   */
  stopMotor() {
    if (!this.motorRunning) return;

    const motorOnAudio = document.getElementById('motorOnSound');
    const motorHumAudio = document.getElementById('motorHumSound');
    const motorHumAudio2 = document.getElementById('motorHumSound2'); // CROSSFADE FIX
    const motorOffAudio = document.getElementById('motorOffSound');

    if (!motorOnAudio || !motorHumAudio || !motorOffAudio) return;

    // CROSSFADE FIX: Clear crossfade timer
    // TO ROLLBACK: Remove this block
    if (this.motorCrossfadeTimer) {
      clearInterval(this.motorCrossfadeTimer);
      this.motorCrossfadeTimer = null;
    }
    // END CROSSFADE FIX

    // Stop any currently playing sounds
    motorOnAudio.pause();
    motorOnAudio.currentTime = 0;
    motorHumAudio.pause();
    motorHumAudio.currentTime = 0;

    // CROSSFADE FIX: Also stop second motor hum element
    // TO ROLLBACK: Remove these two lines
    if (motorHumAudio2) {
      motorHumAudio2.pause();
      motorHumAudio2.currentTime = 0;
    }
    // END CROSSFADE FIX

    // Play motor-off sound once
    motorOffAudio.loop = false;
    motorOffAudio.currentTime = 0;
    motorOffAudio.play().catch(err => console.log('Audio playback failed:', err));

    this.motorRunning = false;
    this.soundEnabled = false;
  }

  /**
   * Toggle sound on/off
   * Returns new state
   * CROSSFADE FIX: Also handles second motor hum element
   * TO ROLLBACK: Remove motorHumAudio2 references
   */
  toggle() {
    const motorHumAudio = document.getElementById('motorHumSound');
    const motorHumAudio2 = document.getElementById('motorHumSound2'); // CROSSFADE FIX

    if (!motorHumAudio) return this.soundEnabled;

    if (this.soundEnabled) {
      motorHumAudio.pause();
      // CROSSFADE FIX: Also pause second element
      if (motorHumAudio2) motorHumAudio2.pause();
      // END CROSSFADE FIX
      this.soundEnabled = false;
    } else {
      motorHumAudio.play().catch(err => console.log('Audio playback failed:', err));
      // CROSSFADE FIX: Resume whichever element was playing
      if (motorHumAudio2 && !motorHumAudio2.paused) {
        motorHumAudio2.play().catch(err => console.log('Audio playback failed:', err));
      }
      // END CROSSFADE FIX
      this.soundEnabled = true;
    }

    return this.soundEnabled;
  }

  /**
   * CROSSFADE FIX: Manage alternating motor hum playback for gapless looping
   * Monitors audio playback and starts the other element before current one ends
   * TO ROLLBACK: Delete this entire method
   */
  startMotorCrossfade(audio1, audio2) {
    // Check every 50ms to see if we need to start the next element
    this.motorCrossfadeTimer = setInterval(() => {
      if (!this.motorRunning) {
        clearInterval(this.motorCrossfadeTimer);
        this.motorCrossfadeTimer = null;
        return;
      }

      const currentAudio = this.currentMotorHum === 1 ? audio1 : audio2;
      const nextAudio = this.currentMotorHum === 1 ? audio2 : audio1;

      // Get duration and current time
      const duration = currentAudio.duration;
      const currentTime = currentAudio.currentTime;

      // Start next element 300ms before current one ends (crossfade overlap)
      // Increased from 100ms to 300ms to account for 50ms check interval + browser audio scheduling latency
      const crossfadeThreshold = duration - 0.3; // 300ms before end

      if (currentTime >= crossfadeThreshold && nextAudio.paused) {
        // Start the next audio element
        nextAudio.currentTime = 0;
        nextAudio.play().catch(err => console.log('Motor hum crossfade failed:', err));

        // Switch to next element as primary
        this.currentMotorHum = this.currentMotorHum === 1 ? 2 : 1;
      }
    }, 50); // Check every 50ms
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
   * Play character printing sound (complete 100ms fragment)
   * Cycles through fragments 0-6 for variety
   */
  playPrintChar() {
    if (!this.soundEnabled) return;

    // Get audio element for current fragment (0-6)
    const audio = document.getElementById(`printChar0${this.currentFragment}`);
    if (!audio) return;

    // Play complete fragment (no pausing needed - it's already 100ms!)
    audio.currentTime = 0;
    audio.play().catch(err => console.error('Play failed:', err));

    // Cycle to next fragment
    this.currentFragment = (this.currentFragment + 1) % this.numFragments;
  }

  /**
   * Play space character printing sound (complete 100ms fragment)
   * Cycles through fragments 0-6 for variety
   */
  playPrintSpace() {
    if (!this.soundEnabled) return;

    // Get audio element for current fragment (0-6)
    const audio = document.getElementById(`printSpace0${this.currentFragment}`);
    if (!audio) return;

    // Play complete fragment (no pausing needed - it's already 100ms!)
    audio.currentTime = 0;
    audio.play().catch(err => console.error('Play failed:', err));

    // Cycle to next fragment
    this.currentFragment = (this.currentFragment + 1) % this.numFragments;
  }

  /**
   * Check if sound is currently enabled
   */
  isEnabled() {
    return this.soundEnabled;
  }

  /**
   * Check if motor is running
   */
  isMotorRunning() {
    return this.motorRunning;
  }
}

// Create global instance
const teletypeAudio = new TeletypeAudio();