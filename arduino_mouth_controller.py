import serial
import time
import threading
import random

# This script requires the 'pyserial' library.
# You can install it via pip: pip install pyserial

# Mapping of English characters to the 5 supported mouth phoneme shapes.
# This is a simplified approximation for animation purposes.
PHONE_MAP = {
    # Vowels create the most distinct shapes
    'a': 'A', 'A': 'A',
    'e': 'E', 'E': 'E',
    'i': 'E', 'I': 'E',
    'o': 'O', 'O': 'O',
    'u': 'U', 'U': 'U',

    # Consonants that use open or smiling shapes
    'c': 'E', 'C': 'E', 'd': 'E', 'D': 'E', 'g': 'E', 'G': 'E',
    'h': 'A', 'H': 'A', 'j': 'E', 'J': 'E', 'k': 'E', 'K': 'E',
    'l': 'E', 'L': 'E', 'n': 'E', 'N': 'E', 'r': 'E', 'R': 'E',
    's': 'E', 'S': 'E', 't': 'E', 'T': 'E', 'y': 'E', 'Y': 'E',
    'z': 'E', 'Z': 'E',

    # Consonants with rounded lips
    'q': 'O', 'Q': 'O',
    'w': 'O', 'W': 'O',

    # Consonants with narrowed lips
    'f': 'U', 'F': 'U',
    'v': 'U', 'V': 'U',

    # Consonants with closed lips
    'b': 'M', 'B': 'M',
    'm': 'M', 'M': 'M',
    'p': 'M', 'P': 'M',
    
    # Pauses between words result in a closed mouth
    ' ': 'M'
}

class ArduinoMouth:
    """
    Manages serial communication with an Arduino to control an LCD mouth display.
    The `say` method is non-blocking and animates the speech in a background thread.
    """

    def __init__(self, port='COM4', baudrate=9600):
        """
        Initializes the controller.
        :param port: The serial port the Arduino is connected to (e.g., 'COM3' on Windows).
        :param baudrate: The baud rate, matching the Arduino's Serial.begin().
        """
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.animation_thread = None

    def connect(self):
        """
        Opens the serial connection to the Arduino.
        Returns True on success, False on failure.
        """
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=1, rtscts=False, dsrdtr=False)
            # It's common for Arduinos to reset upon serial connection.
            # This delay gives it time to initialize before we send data.
            time.sleep(2)
            print(f"Successfully connected to Arduino on {self.port}")
            return True
        except serial.SerialException as e:
            print(f"Error: Could not open serial port {self.port}. Is it correct? {e}")
            return False

    def disconnect(self):
        """Closes the serial connection gracefully."""
        if self.animation_thread and self.animation_thread.is_alive():
            # Wait for any ongoing speech animation to finish
            self.animation_thread.join()
        
        if self.ser and self.ser.is_open:
            try:
                self.ser.write(b'M')  # Send a final 'closed mouth' command
            except serial.SerialException as e:
                print(f"Warning: Could not write to serial port on disconnect. {e}")
            finally:
                self.ser.close()
                print("Serial port closed.")

    def _text_to_phonemes(self, text):
        """Converts an input string to a generator of phoneme characters."""
        for char in text:
            # Yield the corresponding phoneme or 'M' if the character isn't in our map
            yield PHONE_MAP.get(char, 'M')

    def _animate(self, text):
        """
        The private method that runs in a thread to send phoneme commands.
        """
        if not self.ser or not self.ser.is_open:
            print("Error: Cannot animate, serial port is not open.")
            return

        phoneme_sequence = self._text_to_phonemes(text)
        last_phoneme = 'M'  # Start with a closed mouth assumption

        for phoneme in phoneme_sequence:
            # To make the animation look more natural, avoid sending the
            # same mouth shape multiple times in a row.
            if phoneme != last_phoneme:
                self.ser.write(phoneme.encode('ascii'))
                last_phoneme = phoneme
            
            # Use a short, randomized delay between phonemes to make it less robotic
            delay = random.uniform(0.08, 0.15)  # 80ms to 150ms
            time.sleep(delay)

        # After the text is done, ensure the mouth returns to a closed state
        if last_phoneme != 'M':
            time.sleep(0.1)  # Small pause before closing
            self.ser.write(b'M')

    def say(self, text):
        """
        Animates the mouth asynchronously based on the input text from a chatbot.
        This function is non-blocking; it starts the animation and returns immediately.
        
        :param text: The string of text for the mouth to animate.
        """
        if not self.ser or not self.ser.is_open:
            print("Error: Cannot say text, serial port not connected.")
            return
            
        if self.animation_thread and self.animation_thread.is_alive():
            # For simplicity, we ignore new messages if one is already playing.
            # A more advanced version could queue messages.
            print("Warning: Mouth is already moving. Ignoring new message.")
            return

        # Run the animation in a separate thread to avoid blocking the main program
        self.animation_thread = threading.Thread(target=self._animate, args=(text,))
        self.animation_thread.start()

    # The following methods allow the class to be used in a 'with' statement,
    # which automatically handles connecting and disconnecting.
    def __enter__(self):
        self.connect()
        return self
        
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.disconnect()


import os
from dotenv import load_dotenv
from groq import Groq

def main():
    """
    Main function to run an interactive chatbot session with Groq
    and animate the responses on the Arduino.
    """
    # Load environment variables from the .env file
    load_dotenv()
    api_key = os.getenv("GROQ_API_KEY")

    if not api_key:
        print("Error: GROQ_API_KEY not found.")
        print("Please check your .env file for the GROQ_API_KEY.")
        return

    # Initialize the Groq client
    try:
        client = Groq(api_key=api_key)
    except Exception as e:
        print(f"Error initializing API client: {e}")
        return

    # Use a 'with' statement for automatic Arduino connection and disconnection
    try:
        with ArduinoMouth(port='COM4', baudrate=9600) as mouth:
            if not mouth.ser or not mouth.ser.is_open:
                print("\nCould not start chat. Please check the Arduino connection.")
                return

            print("\n🤖 Groq AI Companion is ready. Type 'quit' to exit.")
            print("---------------------------------------------------------")

            while True:
                user_input = input("You: ")
                if user_input.lower() == 'quit':
                    break
                
                try:
                    # Send the user's message to the Groq API
                    response = client.chat.completions.create(
                        model="llama-3.1-8b-instant",  # A standard, fast model on Groq
                        messages=[{"role": "user", "content": user_input}]
                    )
                    
                    ai_response = response.choices[0].message.content
                    print(f"Groq: {ai_response}")

                    # Animate the AI's response on the Arduino face
                    mouth.say(ai_response)

                    # IMPORTANT: Wait for the animation to finish before the next prompt
                    if mouth.animation_thread:
                        mouth.animation_thread.join()

                except Exception as e:
                    print(f"An error occurred while communicating with the API: {e}")

    except Exception as e:
        print(f"An unexpected error occurred: {e}")

    print("---------------------------------------------------------")
    print("Session ended. Goodbye!")


if __name__ == "__main__":
    main()
