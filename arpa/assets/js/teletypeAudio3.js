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
 */

class TeletypeAudio {
  constructor() {
    this.soundEnabled = false;
    this.motorRunning = false;
    this.printCharSoundPosition = 0; // Position in buffer (0-4.5s, cycles)
    this.printSpaceSoundPosition = 0; // Position in buffer (0-4.5s, cycles)

    // Audio channels (3 instances of each for alternation)
    this.charChannels = [];
    this.spaceChannels = [];
    this.currentChannel = 0;
    this.numChannels = 3;

    // TEST: Counter for playing every 10th character
    this.charCounter = 0;
  }

  /**
   * Initialize audio - create 3 channel instances from HTML audio elements
   * Avoids CORS issues with fetch() on file:// URLs
   *
   * TEST: Play sample for 2 seconds to verify audio works
   * TO UNDO: Remove test playback code
   */
  init() {
    const sourceCharAudio = document.getElementById('printChars01');
    const sourceSpaceAudio = document.getElementById('printSpaces01');

    // Create 3 independent audio element instances for each sound (channels)
    for (let i = 0; i < this.numChannels; i++) {
      this.charChannels[i] = sourceCharAudio.cloneNode(true);
      this.spaceChannels[i] = sourceSpaceAudio.cloneNode(true);
    }

    // Verify samples are loaded
    if (sourceCharAudio && sourceCharAudio.readyState >= 2) {
      console.log('✓ Print chars sample loaded');
    } else {
      console.error('✗ Print chars sample NOT loaded');
    }

    if (sourceSpaceAudio && sourceSpaceAudio.readyState >= 2) {
      console.log('✓ Print spaces sample loaded');
    } else {
      console.error('✗ Print spaces sample NOT loaded');
    }

    // TEST: Play 20 fragments before terminal starts
    console.log('TEST: Playing 20 fragments...');
    let count = 0;
    const testPlay = () => {
      if (count >= 20) {
        console.log('TEST: Complete');
        return;
      }

      console.log(`Fragment ${count + 1}/20`);
      const audio = this.charChannels[0];
      audio.currentTime = 0;

      audio.play().then(() => {
        console.log(`Fragment ${count + 1} playing`);
        setTimeout(() => {
          audio.pause();
          console.log(`Fragment ${count + 1} paused`);
          count++;
          setTimeout(testPlay, 200);
        }, 100);
      }).catch(err => console.error('Play error:', err));
    };
    testPlay();

    this.soundEnabled = false;
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

    // Play motor-on sound
    motorOnAudio.currentTime = 0;
    motorOnAudio.play().catch(err => console.log('Audio playback failed:', err));

    // When motor-on finishes, start looping the hum
    motorOnAudio.onended = () => {
      motorHumAudio.loop = true;
      motorHumAudio.currentTime = 0;
      motorHumAudio.play().catch(err => console.log('Audio playback failed:', err));
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

    if (!motorOnAudio || !motorHumAudio || !motorOffAudio) return;

    // Stop any currently playing sounds
    motorOnAudio.pause();
    motorOnAudio.currentTime = 0;
    motorHumAudio.pause();
    motorHumAudio.currentTime = 0;

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
   */
  toggle() {
    const motorHumAudio = document.getElementById('motorHumSound');

    if (!motorHumAudio) return this.soundEnabled;

    if (this.soundEnabled) {
      motorHumAudio.pause();
      this.soundEnabled = false;
    } else {
      motorHumAudio.play().catch(err => console.log('Audio playback failed:', err));
      this.soundEnabled = true;
    }

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
   * Play character printing sound (100ms segment from sample)
   * Uses alternating channels to prevent cutoff
   * WAIT for seek to complete before playing (fixes seeking delay issue)
   *
   * TEST: Only play every 10th character
   * TO UNDO: Remove charCounter check
   */
  playPrintChar() {
    if (!this.soundEnabled || this.charChannels.length === 0) return;

    // TEST: Only play every 10th character
    this.charCounter++;
    if (this.charCounter % 10 !== 0) return;

    console.log('Playing sample at position', this.printCharSoundPosition, 'on channel', this.currentChannel);

    // Get audio element for current channel
    const audio = this.charChannels[this.currentChannel];

    // TEST: Play from position 0 (no seeking) to test if 100ms pause works
    audio.currentTime = 0;
    console.log('Starting playback from position 0');

    audio.play().then(() => {
      console.log('Playing started');
    }).catch(err => console.error('Play failed:', err));

    // Stop after 100ms
    setTimeout(() => {
      audio.pause();
      console.log('Paused after 100ms');
    }, 100);

    // Advance position for variety (cycle through 0-4.5s)
    this.printCharSoundPosition += 0.1;
    if (this.printCharSoundPosition >= 4.5) {
      this.printCharSoundPosition = 0;
    }

    // Alternate to next channel
    this.currentChannel = (this.currentChannel + 1) % this.numChannels;
  }

  /**
   * Play space character printing sound (100ms segment from sample)
   * Uses alternating channels to prevent cutoff
   * WAIT for seek to complete before playing (fixes seeking delay issue)
   *
   * TEST: Only play every 10th character
   * TO UNDO: Remove charCounter check
   */
  playPrintSpace() {
    if (!this.soundEnabled || this.spaceChannels.length === 0) return;

    // TEST: Only play every 10th character
    this.charCounter++;
    if (this.charCounter % 10 !== 0) return;

    console.log('Playing space sample at position', this.printSpaceSoundPosition, 'on channel', this.currentChannel);

    // Get audio element for current channel
    const audio = this.spaceChannels[this.currentChannel];

    // TEST: Play from position 0 (no seeking) to test if 100ms pause works
    audio.currentTime = 0;
    console.log('Starting space playback from position 0');

    audio.play().then(() => {
      console.log('Space playing started');
    }).catch(err => console.error('Play failed:', err));

    // Stop after 100ms
    setTimeout(() => {
      audio.pause();
      console.log('Space paused after 100ms');
    }, 100);

    // Advance position for variety (cycle through 0-4.5s)
    this.printSpaceSoundPosition += 0.1;
    if (this.printSpaceSoundPosition >= 4.5) {
      this.printSpaceSoundPosition = 0;
    }

    // Alternate to next channel
    this.currentChannel = (this.currentChannel + 1) % this.numChannels;
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