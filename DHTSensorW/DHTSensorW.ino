//GTT Arduino Tempearature and Humidity Demo 
//Arduino Uno with Matrix Orbital GTT35A and DHT22
//Created by R Malinis, 25/05/2018
//support@matrixorbital.ca
//www.matrixorbital.ca/appnotes

#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
#include <EthernetClient.h>
#include <EthernetServer.h>
#include <EthernetUdp.h>

#include <gtt.h>
#include <gtt_device.h>
#include <gtt_enum.h>
#include <gtt_events.h>
#include <gtt_ext_types.h>
#include <gtt_packet_builder.h>
#include <gtt_parser.h>
#include <gtt_protocol.h>
#include <gtt_text.h>

#include <Wire.h>
#include <SimpleDHT.h>

#include "GTTProject12.c"
#include "GTTProject12.h"
#include <stdlib.h>
#define I2C_Address 0x28  //Define default 8bit I2C address of 0x50 >> 1 for 7bit Arduino

// Buffer for incoming data
uint8_t rx_buffer[128]; 

// Buffer for outgoing data
uint8_t tx_buffer[128]; 

gtt_device gtt; //Declare the GTT device

typedef enum
{
  NOGraph =0,
  TempGraph=1,
  HumiGraph=2
} GraphScreen;

// DHT11
int pinDHT22 = 8;
SimpleDHT22 dht22;
byte defaultMaxHumidity;
bool MaxHumidityScreen = false; 
GraphScreen Graph = NOGraph;

void LiveGraph(float temperature, float humidity)
{
  if (Graph == NOGraph)
    return;    
  
  Serial.println("drawing LIVE graph");

  if (Graph == TempGraph)
    gtt_live_t_graph_t_live_data0_push_data(&gtt, temperature);
  else 
    gtt_live_h_graph_h_live_data0_push_data(&gtt, humidity);

  return;
}

void GTT25ButtonHandler(gtt_device* device, uint16_t ObjectID, uint8_t State)
{     
  //Serial.print("button handler: ");          
  //Serial.print(ObjectID);        
  //Serial.print(" state: ");          
  //Serial.println(State);        
  
  if (State != 1)
    return;
 
  MaxHumidityScreen = false;
  Graph = NOGraph;                  
  switch (ObjectID)
  {
    case id_screen2_triangle_button_1:
      defaultMaxHumidity++;
      break;
    case id_screen2_triangle_button_2:
      defaultMaxHumidity--;      
      break;
    case id_screen1_max_btn:
      break;
    case id_screen1_image_button_1:      
      Graph = TempGraph;
      return;
      break;      
    case id_screen1_image_button_2:      
      Graph = HumiGraph;
      return;
      break;           
    // the following buttons take care of themselves as designed in the GTT Designer
    // only used here for serial debug
    case id_screen2_home_btn:       
    case id_screen3_home_btn:
    case id_livehgraph_home_btn:
    case id_hgraph_1hr_home_btn:
    case id_hgraph_24hr_home_btn:
    case id_hgraph_1wk_home_btn:
    case id_livetgraph_home_btn:   
      //Serial.println("==== back to HOME screen");                
      return;
      break;   
    case id_livetgraph_1_hr_btn:
    case id_tgraph_1hr_1_hr_btn:   
    case id_tgraph_24hr_1_hr_btn:
    case id_tgraph_1wk_1_hr_btn:  
    case id_livehgraph_1_hr_btn:
    case id_hgraph_1hr_1_hr_btn:   
    case id_hgraph_24hr_1_hr_btn:
    case id_hgraph_1wk_1_hr_btn:   
      //Serial.println("==== displaying 1 HOUR");     
      return;
      break;
    case id_livetgraph_24_hr_btn:
    case id_tgraph_1hr_24_hr_btn:   
    case id_tgraph_24hr_24_hr_btn:
    case id_tgraph_1wk_24_hr_btn:     
    case id_livehgraph_24_hr_btn:
    case id_hgraph_1hr_24_hr_btn:   
    case id_hgraph_24hr_24_hr_btn:
    case id_hgraph_1wk_24_hr_btn:           
      //Serial.println("==== displaying 24 HOUR");       
      return;
      break;
    case id_livetgraph_1_wk_btn:
    case id_tgraph_1hr_1_wk_btn:
    case id_tgraph_24hr_1_wk_btn:    
    case id_tgraph_1wk_1_wk_btn:    
    case id_livehgraph_week_btn:
    case id_hgraph_1hr_week_btn:
    case id_hgraph_24hr_week_btn:    
    case id_hgraph_1wk_week_btn:     
      //Serial.println("==== displaying 1 WEEK");  
      return;
      break;
  }  
  
  char buf[6] = {0};
  int16_t humid = defaultMaxHumidity;  
  sprintf(buf, "%d", humid); //Convert the humidity value to a string  
  gtt_set_screen2_humi_label_1_text(&gtt, gtt_make_text_ascii(buf)); //Update the GTT label    
  //Serial.println("defaultMaxHumidity is set");
  //Serial.println(humid);
}

void HumidityCheck(float humidityVal)
{
  if (humidityVal > defaultMaxHumidity)
  {
    //Serial.print("max: "); Serial.print((int)defaultMaxHumidity); 
    //Serial.print(" current:"); Serial.println((int)humidityVal); 
    if (MaxHumidityScreen == false)
    {
      gtt_run_script(&gtt, (char*)("GTTProject12\\Screen3\\Screen3.bin"));
      MaxHumidityScreen = true;        
      delay(2000);
    }
  }

  if (MaxHumidityScreen == true &&  (humidityVal < defaultMaxHumidity))
  {      
      gtt_run_script(&gtt, (char*)("GTTProject12\\Screen1\\Screen1.bin"));      
      MaxHumidityScreen = false;        
      delay(2000);    
  }
  return;
}

float TempData[10] = {0,0,0,0,0,0,0,0,0,0};
float HumiData[10] = {0,0,0,0,0,0,0,0,0,0};

bool OldDataLoaded = false;
 
void UpdateDataCollection(float temperature, float humidity)
{
      float avgTempData, avgHumiData;
      static byte counter20sec = 0; // 180 data points = 1hr
      static byte counter8mins = 0; // 180 data points = 24hrs
      static byte counter56mins = 0;// 180 data points = 1week
      //DataSourceType dataSource;
      static byte HrMaxH = 15;
      static byte HrMinH = 25;
      static byte HrMaxT = 20;
      static byte HrMinT = 25;
      static byte T24HrMaxH = 15;
      static byte T24HrMinH = 25;
      static byte T24HrMaxT = 20;
      static byte T24HrMinT = 25;    
      
      //debug
      static byte count20secP = 0;
      static byte count8minP = 0;
      static byte count56minP = 0;  
                
      LiveGraph(temperature, humidity);
      //Serial.print("Data (not collected): ");
      //Serial.print((float)temperature); Serial.print(" *C, ");
      //Serial.print((float)humidity); Serial.println(" RH%");     

      eStatusCode r;
      if (OldDataLoaded == false)
      {
        OldDataLoaded = true;
        r = gtt25_dataset_load(&gtt, id_tgraph_24hr_24_hr_chart_data0, gtt_make_text_ascii("Temp24hrMax.dat"));  
        r = gtt25_dataset_load(&gtt, id_tgraph_24hr_24_hr_chart_data1, gtt_make_text_ascii("Temp24hrMin.dat"));        
        r = gtt25_dataset_load(&gtt, id_hgraph_24hr_24_hr_chart_data0, gtt_make_text_ascii("Humidity24hrMax.dat"));  
        r = gtt25_dataset_load(&gtt, id_hgraph_24hr_24_hr_chart_data1, gtt_make_text_ascii("Humidity24hrMin.dat"));        
        r = gtt25_dataset_load(&gtt, id_tgraph_1wk_1_wk_chart_data0, gtt_make_text_ascii("Temp1WkMax.dat"));  
        r = gtt25_dataset_load(&gtt, id_tgraph_1wk_1_wk_chart_data1, gtt_make_text_ascii("Temp1WkMin.dat"));        
        r = gtt25_dataset_load(&gtt, id_hgraph_1wk_1_wk_chart_data0, gtt_make_text_ascii("Humidity1WkMax.dat"));  
        r = gtt25_dataset_load(&gtt, id_hgraph_1wk_1_wk_chart_data1, gtt_make_text_ascii("Humidity1WkMin.dat"));        
        if (r != eStatusCode_Success)
        {
          Serial.println("Check uSD card");
        }
      }

      // collect data for averaging after 20 seconds (10 readings)
      TempData[counter20sec] = temperature; 
      HumiData[counter20sec] = humidity;
      //Serial.print("temp: ");  
      //Serial.print(temperature);  
      //Serial.print(" humidity: ");  
      //Serial.print(humidity);        

      Serial.print("  2 sec counter: ");  
      Serial.println(counter20sec);              
      counter20sec++;
      if (counter20sec < 10) // 10x 2sec refresh rate = 20 seconds
        return;  

      // take the avg of the 10 data points, that is what we will set as hr data points
      avgTempData = 0;
      avgHumiData = 0;
      for (byte i=0; i < 10; i++)
      {
        avgTempData += TempData[i]; 
        avgHumiData += HumiData[i];
      } 
      avgTempData = avgTempData/10; 
      avgHumiData = avgHumiData/10;
      Serial.print("AVG temp: ");  
      Serial.print(avgTempData);  
      Serial.print(" AVG humidity: ");  
      Serial.println(avgHumiData);             
      
      Serial.print("Data collection: 1hr data points: ");  
      Serial.print(count20secP);      
      Serial.println(" ");      
      
      Serial.print("Data collection: 24hr data points: ");      
      Serial.print(count8minP);  
      Serial.println(" ");      

      Serial.print("Data collection: 1wk data points: ");      
      Serial.print(count56minP); 
      Serial.println(" ");      
      
      count20secP++;
      counter20sec = 0;
      counter8mins++;
      
      // update 1hr graph
      gtt_t_graph_1_h_r_1_hr_chart_data0_push_data(&gtt, avgTempData);            
      if ((byte)temperature > HrMaxT)
        HrMaxT = (byte)temperature;
      if ((byte)temperature < HrMinT)
        HrMinT = (byte)temperature;

      gtt_h_graph_1_h_r_1_hr_chart_data0_push_data(&gtt, avgHumiData);            
      if ((byte)humidity > HrMaxH)
        HrMaxH = (byte)humidity;
      if ((byte)humidity < HrMinH)
        HrMinH = (byte)humidity;  
      
      if (counter8mins > 23) // <-- 23
      {
        //Serial.print("Data collection: 24hr data points: ");      
        //Serial.print(count8minP);  
        //Serial.print(" ");      
        count8minP++;
        counter8mins = 0;
        counter56mins++;

        // update 24hr graph, 2 plots, temp min and max
        gtt_t_graph_24_h_r_24_hr_chart_data0_push_data(&gtt, HrMaxT);
        gtt_t_graph_24_h_r_24_hr_chart_data1_push_data(&gtt, HrMinT);
        
        r = gtt25_dataset_save(&gtt, id_tgraph_24hr_24_hr_chart_data0, gtt_make_text_ascii("Temp24hrMax.dat"));  
        r = gtt25_dataset_save(&gtt, id_tgraph_24hr_24_hr_chart_data1, gtt_make_text_ascii("Temp24hrMin.dat"));  

        if ((byte)temperature > T24HrMaxT)
          T24HrMaxT = (byte)temperature;
        if ((byte)temperature < T24HrMinT)
          T24HrMinT = (byte)temperature;

        // update 24hr graph, 2 plots, humidity min and max
        gtt_h_graph_24_h_r_24_hr_chart_data0_push_data(&gtt, HrMaxH);
        gtt_h_graph_24_h_r_24_hr_chart_data1_push_data(&gtt, HrMinH);
               
        r = gtt25_dataset_save(&gtt, id_hgraph_24hr_24_hr_chart_data0, gtt_make_text_ascii("Humidity24hrMax.dat"));        
        r = gtt25_dataset_save(&gtt, id_hgraph_24hr_24_hr_chart_data1, gtt_make_text_ascii("Humidity24hrMin.dat"));        

        if ((byte)humidity > T24HrMaxH)
          T24HrMaxH = (byte)humidity;
        if ((byte)humidity < T24HrMinH)
          T24HrMinH = (byte)humidity;
        
        if (counter56mins > 6) //<-- 6
        {          
          //Serial.print("Data collection: 1wk data points: ");      
          //Serial.print(count56minP); 
          //Serial.print(" ");      
          count56minP++;
          counter56mins = 0;

          // update Week graph
          gtt_t_graph_1_w_k_1_wk_chart_data0_push_data(&gtt, T24HrMaxT);
          gtt_t_graph_1_w_k_1_wk_chart_data1_push_data(&gtt, T24HrMinT);
          gtt25_dataset_save(&gtt, id_tgraph_1wk_1_wk_chart_data0, gtt_make_text_ascii("Temp1WkMax.dat")); //< change file names      
          gtt25_dataset_save(&gtt, id_tgraph_1wk_1_wk_chart_data1, gtt_make_text_ascii("Temp1WkMin.dat")); //< change file names       
          
          gtt_h_graph_1_w_k_1_wk_chart_data0_push_data(&gtt, T24HrMaxH);
          gtt_h_graph_1_w_k_1_wk_chart_data1_push_data(&gtt, T24HrMinH);          
          gtt25_dataset_save(&gtt, id_hgraph_1wk_1_wk_chart_data0, gtt_make_text_ascii("Humidity1WkMax.dat"));//< change file names                  
          gtt25_dataset_save(&gtt, id_hgraph_1wk_1_wk_chart_data1, gtt_make_text_ascii("Humidity1WkMin.dat"));//< change file names                  
        }
      }  
      //Serial.println(" ");                
      //debug
      count20secP %= 180;
      count8minP %= 180;
      count56minP %= 180;     
}

void UpdateHomeScreen(float temperature, float humidity)
{
  char buf[6];
  char wholeTemp[3];
  char fracTemp[2];
  float temp = (float)temperature;
  int humid = (float)humidity;
  
  dtostrf(temp, 2, 2, buf);
  for (int i=0; i<3;i++)
    wholeTemp[i] = buf[i];
  wholeTemp[2] = 0; // null
  fracTemp[0] = buf[3];
  fracTemp[1] = 0; // null   
  
  gtt_set_screen1_temp_label_1_text(&gtt, gtt_make_text_ascii(wholeTemp)); //Update the GTT label
  gtt_set_screen1_temp_dec_label_1_text(&gtt, gtt_make_text_ascii(fracTemp)); //Update the GTT label
  
  sprintf(buf,"%d",humid);
  gtt_set_screen1_humi_label_1_text(&gtt, gtt_make_text_ascii(buf)); //Update the GTT label
}

byte ReadSensor(float*  temperature, float* humidity)
{    
    int err = SimpleDHTErrSuccess;
    if ((err = dht22.read2(pinDHT22, temperature, humidity, NULL)) != SimpleDHTErrSuccess) {
      //Serial.print("Read DHT22 failed, err="); Serial.println(err);delay(2000);
      return 0;
    }    
    return 1;
}

void setup() {

  //Setup I2C bus
  gtt.Write = i2cWrite; //Set the write function
  gtt.Read = i2cRead; //Set the read function
  gtt.rx_buffer = rx_buffer; //Declare a buffer for input data
  gtt.rx_buffer_size = sizeof(rx_buffer); //Declare the size of the input buffer
  gtt.tx_buffer = tx_buffer; //Declare a buffer for output data
  gtt.tx_buffer_size = sizeof(tx_buffer); //Declare the size of the output buffer
  
  gtt25_set_button_clickhandler(&gtt, GTT25ButtonHandler);       
  Wire.begin(); //Begin I2C communication

  Serial.begin(115200);
  gtt_reset(&gtt);
  delay(4000);
  Serial.println("Start..");
  defaultMaxHumidity = 70;     
}

unsigned short count = 0;
unsigned short dataColCount = 0;

void loop() {
  // read without samples.
  byte temperature = 0;
  byte humidity = 0;  
  byte collectionRate = 2; // 30
  count++;
 
  if (count > 2100) // cycle delay for DHT, refresh is 0.5HZ (2 seconds)
  {
    count = 0;
    dataColCount++;
    float temperature = 0;
    float humidity = 0;
    if (ReadSensor(&temperature, &humidity) == 1) 
    {
      UpdateHomeScreen(temperature, humidity);
      HumidityCheck(humidity);    
      if (dataColCount > collectionRate) 
      {
        UpdateDataCollection(temperature, humidity);
        dataColCount = 0; // restart delay count
      }
    }
  }
  
  gtt_parser_process(&gtt); 
}

int i2cWrite(gtt_device* gtt_device, char* data, size_t data_length) {//Write an array of bytes over i2c

  size_t len = data_length;
  byte offset = 0;
  if (len > 32)
    len = 32;

  while (data_length)
  {
    Wire.beginTransmission(I2C_Address);
    for (int i = 0; i < len; i++) 
    {
      Wire.write(data[i+offset]);
      delay(1);
    }
    Wire.endTransmission();
    data_length = data_length - len;
    len = data_length; 
    if (len > 32) 
      len = 32;
    offset += 32;
  };  
  return 0;
}

byte i2cRead(gtt_device* gtt_device) { //Wait for one byte to be read over i2c
  byte data;
  Wire.beginTransmission(I2C_Address);
  Wire.requestFrom(I2C_Address, 1);
  if (Wire.available() < 1)
  {
    return -1;
  }
  else {
    data = Wire.read();
    //Serial.println(data);
    return data;
  }
}
