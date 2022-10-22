
/*********************************************************************************************\
 * Arduino project "trimix-analyzer.ino" de Yves Cazé E3 ASPTT Lyon
\*********************************************************************************************/
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <RunningAverage.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x3F, 20, 4);  // set the LCD address to 0x3F for a 20 chars and 4 line display
Adafruit_ADS1115 ads;        // convertisseur analogique --> digital ADS1115

// variables will change:
char  version[] = "22.10.13";
float TensionCalib = 0;      // mise a 0 de la tension de calibrage de la cellule / setting to 0 of the cell calibration voltage
float voltage = 0;           // tension mesuree sur cellule / voltage measured on cell
float wheatstone = 0;        // tension sur pont de wheatsone / wheatsone bridge voltage

float gain = 0.03125;        // gain du convertisseur ADS1115 / ADS1115 converter gain
float calibMD62 = 661.26;    // valeur de la tension du pont avec 100% helium / bridge voltage value with 100% helium
float WheatCalib = 10;       // mise a 0 de la tension de calibrage du pont à l'air / setting to 0 of the calibration voltage of the air bridge
float CorrFroid = 0 ;        // correction de lecture du fait de lecture a froid (capteur pas assez chaud) / reading correction due to cold reading (sensor not hot enough)
unsigned long time;          // mesure du temps depuis allumage pour ajuster correction / measurement of time since ignition to adjust correction

RunningAverage RA0(10);      // moyennage O2 sur 10 valeurs / O2 averaging on 10 values
RunningAverage RA1(10);      // moyennage He sur 10 valeurs / He averaging on 10 values


/*********************************************************************************************\
 * SETUP
\*********************************************************************************************/
void setup()
{  
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);  
  
  lcd.init();
  lcd.backlight();
  lcd.print("  Trimix Analyser");
  lcd.setCursor(6,1);
  lcd.print(version);
  delay(1000);

  ads.setGain(GAIN_FOUR); // 4x gain 1 bit = 0.03125mV
  ads.begin();

  int16_t adc0;  // Oxygen sensor
  int16_t adc1;  // "Helium" sensor 

  adc0 = ads.readADC_Differential_0_1();
  RA0.addValue(adc0);
  voltage = abs(RA0.getAverage()*gain);
  
  adc1 = ads.readADC_Differential_2_3();
  RA1.addValue(adc1);
  wheatstone = RA1.getAverage()*gain;
 
  // voltage display 
  lcd.setCursor(0,2);
  lcd.print("V O2 = ");
  lcd.print(voltage,2);
  lcd.print("mV");
  lcd.setCursor(0,3);
  lcd.print("V He = ");
  lcd.print(wheatstone,2);
  lcd.print("mV");
  delay(3000);

  // Begin calibration
  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print(" Calibrating...");
  lcd.setCursor(0,2);
  lcd.print(" (using air)");
  
  // determination de la tension moyenne de la cellule à l'air libre /
  // determination of the average voltage of the cell in the open air 
  int i = 0;
  float tensionMoyenne = 0;
  
  for(i = 1; i <10 or (abs (voltage - (tensionMoyenne / (i-1)))) > 0.001; i++) 
  {
    adc0 = ads.readADC_Differential_0_1();
    RA0.addValue(adc0);
    voltage = abs(RA0.getAverage()*gain);
    tensionMoyenne = tensionMoyenne + voltage;
    delay(200);
  }

  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print(" Calibration OK");
  tensionMoyenne = tensionMoyenne / (i - 1);
  TensionCalib = tensionMoyenne;
  lcd.setCursor(0,2);
  lcd.print(" V calib = ");
  lcd.print(TensionCalib,2);
  lcd.print("mV");
  // End calibration
   
  delay(2000);

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Preheating the");
  lcd.setCursor(0,1);
  lcd.print("  Helium sensor...");
  delay(500);
   
  while(wheatstone > 10)
  {
    adc1 = ads.readADC_Differential_2_3();
    RA1.addValue(adc1);
    wheatstone = RA1.getAverage()*gain;
    lcd.setCursor(4,3);    
    lcd.print("Vbridge=");
    lcd.print(wheatstone,0);
    lcd.print("mV  "); 
    delay(50);
  }
         
  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print(" He sensor OK");           
  delay(2000);
  lcd.clear();

}


void loop()
{
  // put your main code here, to run repeatedly:
  
  int16_t adc0;
  int16_t adc1;
  adc0 = ads.readADC_Differential_0_1();
  adc1 = ads.readADC_Differential_2_3();
  
  time = millis();      // temps en ms depuis démarrage programme / time in ms since program startup

  RA0.addValue(adc0);
  voltage = abs(RA0.getAverage()*gain);

  RA1.addValue(adc1);
  wheatstone = RA1.getAverage()*gain;

  float nitrox = 0;
  int MOD = 0;                  // valeur de MOD du mélange / MOD value of the mixture
  int EAD = 0;                  // prof equivalente Air pour narcose 30m / prof equivalent Air for narcosis 30m
  float helium = 0;

  nitrox = voltage * (20.9 / TensionCalib);
  MOD = 10 * ( (160/nitrox) - 1);

  lcd.setCursor(0,0);
  if (voltage > 1)
  {
    lcd.print("Oxygen = ");
    lcd.print(nitrox,1);
    lcd.print("%   ");
    lcd.setCursor(12,2);
    lcd.print(" MOD");
    lcd.print(MOD);
    lcd.print("m");

    if (MOD < 100)
    {
      lcd.print(" "); 
    }
  }
  else
  {
    lcd.print("!  Cell O2 Error   !");
  }

//  lcd.setCursor(0,3);
//  lcd.print("Vbridge=");
//  lcd.print(wheatstone,0);
//  lcd.print("mV  ");

  wheatstone = wheatstone - WheatCalib;  
    
  if (time < 480000) {  CorrFroid = 1 ; }     // correction de la tension lue en fonction du temps de chauffe du capteur 
  if (time < 360000) {  CorrFroid = 2 ; }     // correction of the read voltage according to the heating time of the sensor
  if (time < 300000) {  CorrFroid = 3 ; }
  if (time < 270000) {  CorrFroid = 4 ; }
  if (time < 240000) {  CorrFroid = 5 ; }
  if (time < 210000) {  CorrFroid = 6 ; }
  if (time < 180000) {  CorrFroid = 7 ; }
  if (time < 165000) {  CorrFroid = 8 ; }
  if (time < 150000) {  CorrFroid = 9 ; }
  if (time < 120000) {  CorrFroid = 10 ; }
  if (time < 105000) {  CorrFroid = 11 ; }
  if (time < 90000)  {  CorrFroid = 12 ; }
  if (time < 80000)  {  CorrFroid = 13 ; }
  if (time < 70000)  {  CorrFroid = 14 ; }
  if (time < 60000)  {  CorrFroid = 15 ; }
  if (time < 50000)  {  CorrFroid = 16 ; }
  if (time < 40000)  {  CorrFroid = 17 ; }
  if (time < 30000)  {  CorrFroid = 18 ; }
  
  wheatstone = wheatstone - CorrFroid;            // ajustement car capteur pas assez chaud / adjustment because sensor not hot enough

  lcd.setCursor(0,1);    
  lcd.print("Helium = ");
  helium = 100 * wheatstone / calibMD62;

  if (helium > 50)
  {
    helium = helium * (1 + (helium - 50) * 0.4 / 100);
  }

  if (helium > 2)
  {
    lcd.print(helium,1);
    lcd.print("%    "); 
  }
  else
  {
    helium = 0;
    lcd.print("0%    ");  
  }

  lcd.setCursor(0,2);  
  if (helium > 0)
  {
    lcd.print("Trimix ");
    lcd.print(nitrox,0);
    lcd.print("/");
    lcd.print(helium,0);
    lcd.print(" ");
    lcd.setCursor(12,3);
    lcd.print(" EAD");
    EAD = 10 * ( 100 * 3.2 / (100 - helium - nitrox) -1 );
    lcd.print(EAD);
    lcd.print("m");  
    if (EAD < 100)
    {
      lcd.print(" "); 
    } 
  }
  else
  {
    lcd.print("Nitrox ");
    lcd.print(nitrox,0);
    lcd.print("    ");
    lcd.setCursor(12,3);
    lcd.print("       ");
  }

  // Add a delay
  delay (50);
}
