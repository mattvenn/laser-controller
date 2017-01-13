#include <EEPROM.h>

void setupEEPROM()
{
    EEPROM.begin(512);
}

//This function will write a 2 byte (16bit) unsigned int to the eeprom at
//the specified address to address + 1.
void EEPROMWriteInt(int address, unsigned int value)
{
  //Decomposition from an int to 2 bytes by using bitshift.
  //One = Most significant -> two = Least significant byte
  byte two = (value & 0xFF);
  byte one = ((value >> 8) & 0xFF);

  //Write the 2 bytes into the eeprom memory.
  EEPROM.write(address, two);
  EEPROM.write(address + 1, one);
  EEPROM.commit();
}

unsigned int EEPROMReadInt(int address)
{
  //Read the 2 bytes from the eeprom memory.
  long two = EEPROM.read(address);
  long one = EEPROM.read(address + 1);

  //Return the recomposed int by using bitshift.
  return ((two << 0) & 0xFF) + ((one << 8) & 0xFFFF); 
}
