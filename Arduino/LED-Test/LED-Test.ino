// Simple LED test to verify NeoPixel functionality
// Cycles through Red, Green, Blue, White, then off

#include <Adafruit_NeoPixel.h>

#define RGB_PIN 5
#define NEOPIXEL_TYPE (NEO_GRB + NEO_KHZ800)

Adafruit_NeoPixel pixel = Adafruit_NeoPixel(1, RGB_PIN, NEOPIXEL_TYPE);

void setup() {
  Serial0.begin(115200);
  delay(1000);
  
  Serial0.println("========================================");
  Serial0.println("       NeoPixel LED Test - DIAGNOSTIC");
  Serial0.println("========================================");
  Serial0.print("Pin: ");
  Serial0.println(RGB_PIN);
  Serial0.print("Pixel Type: NEO_GRB + NEO_KHZ800");
  Serial0.println();
  
  pixel.begin();
  pixel.setBrightness(255); // Full brightness for testing
  pixel.show(); // Initialize all pixels to 'off'
  
  Serial0.println("LED initialized - Starting color cycle");
  Serial0.println("If LED doesn't change, try:");
  Serial0.println("  1. Check wiring to pin 5");
  Serial0.println("  2. Test with different pixel type (NEO_RGB)");
  Serial0.println("  3. Hardware may be faulty");
  Serial0.println("========================================");
}

void loop() {
  // Off first
  Serial0.println("OFF - All channels 0");
  pixel.setPixelColor(0, 0, 0, 0);
  pixel.show();
  delay(3000);
  
  // Red only
  Serial0.println("RED - Testing Red channel (255, 0, 0)");
  pixel.setPixelColor(0, 255, 0, 0);
  pixel.show();
  delay(3000);
  
  // Off
  Serial0.println("OFF");
  pixel.setPixelColor(0, 0, 0, 0);
  pixel.show();
  delay(1000);
  
  // Green only
  Serial0.println("GREEN - Testing Green channel (0, 255, 0)");
  pixel.setPixelColor(0, 0, 255, 0);
  pixel.show();
  delay(3000);
  
  // Off
  Serial0.println("OFF");
  pixel.setPixelColor(0, 0, 0, 0);
  pixel.show();
  delay(1000);
  
  // Blue only
  Serial0.println("BLUE - Testing Blue channel (0, 0, 255)");
  pixel.setPixelColor(0, 0, 0, 255);
  pixel.show();
  delay(3000);
  
  // Off
  Serial0.println("OFF");
  pixel.setPixelColor(0, 0, 0, 0);
  pixel.show();
  delay(1000);
  
  // Dim green to see if it's stuck
  Serial0.println("DIM GREEN (0, 50, 0) - Testing if LED responds to brightness");
  pixel.setPixelColor(0, 0, 50, 0);
  pixel.show();
  delay(3000);
  
  Serial0.println("========================================");
  Serial0.println("Cycle complete. Watch for changes.");
  Serial0.println("If LED stays green always = HARDWARE FAULT");
  Serial0.println("========================================");
  delay(2000);
}
