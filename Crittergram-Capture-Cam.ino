//Crittergram Capture Cam
//by Julius Schmiedel
//for http://makezine.com/weekendprojects/

//Camera definitions

#define VC0706_PROTOCOL_SIGN 			0x56
#define VC0706_SERIAL_NUMBER 			0x00
#define VC0706_COMMAND_RESET			0x26
#define VC0706_COMMAND_GEN_VERSION		0x11
#define VC0706_COMMAND_TV_OUT_CTRL		0x44
#define VC0706_COMMAND_OSD_ADD_CHAR		0x45
#define VC0706_COMMAND_DOWNSIZE_SIZE		0x53
#define VC0706_COMMAND_READ_FBUF		0x32
#define FBUF_CURRENT_FRAME			0
#define FBUF_NEXT_FRAME				0
#define VC0706_COMMAND_FBUF_CTRL		0x36
#define VC0706_COMMAND_COMM_MOTION_CTRL		0x37
#define VC0706_COMMAND_COMM_MOTION_DETECTED	0x39
#define VC0706_COMMAND_POWER_SAVE_CTRL		0x3E
#define VC0706_COMMAND_COLOR_CTRL		0x3C
#define VC0706_COMMAND_MOTION_CTRL		0x42
#define VC0706_COMMAND_WRITE_DATA		0x31
#define VC0706_COMMAND_GET_FBUF_LEN		0x34
#define READ_DATA_BLOCK_NO			56
unsigned char 	tx_counter;
unsigned char 	tx_vcbuffer[20];
bool		tx_ready;
bool		rx_ready;
unsigned char 	rx_counter;
unsigned char 	VC0706_rx_buffer[80]; 
uint32_t 	frame_length=0;
uint32_t 	vc_frame_address =0;
uint32_t 	last_data_length=0;



// SD card stuff

#include <SD.h>
File myFile;

// where is the SD card connected? seeed = SS, ethernet shield = 4
const uint8_t SdChipSelect = SS;

char myFileName[16];
int myFileNr=1;


void setup() 
{

        // we are using the standard SD library. pin 4 = ardunio ethernet shield - pin 10 = seeed sd shield
        SD.begin(SdChipSelect);
	
	Serial.begin(115200);

        // set jpeg quality
	VC0706_compression_ratio(66);
	delay(100);

	capture_photo("test.jpg");

        // turn on power for PIR on pin 8
        pinMode(8, OUTPUT);
        digitalWrite(8, HIGH);

}




void loop() 
{

  // create a new file each and every time
  while (myFileNr != 0) {
    sprintf(myFileName, "CCC%03d.jpg", myFileNr);
    if (SD.exists(myFileName) == false) break;
    myFileNr++;
  }
  
  // wait for movement - PIR out will turn HIGH
    while (digitalRead(7) == LOW) {
       delay(1);
      } 
  
  // take picture
    capture_photo(myFileName);
    delay(10);
  
}



/*******************************************************************************
 * Function Name  : capture_photo
 * Description	  : capture a photo and store the file named temp.jpg into SD
 *					
 * Input		  : None
 *					
 * Output		  : None
 * Return		  : None
 *******************************************************************************/
void capture_photo(char myFileName[]) {	

        // make sure video engine is running
  	VC0706_frame_control(3);
	delay(10);
	
        // Check to see if the file exists: 
	// if exists,delete the file:
	if(SD.exists(myFileName)) SD.remove(myFileName);
	

        // freeze video enginge
	VC0706_frame_control(0);
	delay(10);
	rx_ready=false;
	rx_counter=0;
	
	Serial.end();			// clear all rx buffer
	delay(5);
	
	Serial.begin(115200);

	//get frame buffer length
	VC0706_get_framebuffer_length(0);
	delay(10);
	buffer_read();
	
	// store frame buffer length for coming reading
	frame_length=(VC0706_rx_buffer[5]<<8)+VC0706_rx_buffer[6];
	frame_length=frame_length<<16;
	frame_length=frame_length+(0x0ff00&(VC0706_rx_buffer[7]<<8))+VC0706_rx_buffer[8];

	vc_frame_address =READ_DATA_BLOCK_NO;
		
	myFile=SD.open(myFileName, FILE_WRITE);	
	while(vc_frame_address<frame_length){	
		VC0706_read_frame_buffer(vc_frame_address-READ_DATA_BLOCK_NO, READ_DATA_BLOCK_NO);
		delay(9);

		//get the data with length=READ_DATA_BLOCK_NObytes 
		rx_ready=false;
		rx_counter=0;
		buffer_read();

		// write data to file
		myFile.write(VC0706_rx_buffer+5,READ_DATA_BLOCK_NO);
	
		//read next READ_DATA_BLOCK_NO bytes from frame buffer
		vc_frame_address=vc_frame_address+READ_DATA_BLOCK_NO;
	
		}

	// get the last data
	vc_frame_address=vc_frame_address-READ_DATA_BLOCK_NO;

	last_data_length=frame_length-vc_frame_address;

	
	VC0706_read_frame_buffer(vc_frame_address,last_data_length);
	delay(9);
	//get the data 
	rx_ready=false;
	rx_counter=0;
	buffer_read();
			
	myFile.write(VC0706_rx_buffer+5,last_data_length);
	
	myFile.close();

        // restart video enginge
	VC0706_frame_control(3);
	delay(10);
}


/*******************************************************************************
 * Function Name  : VC0706_read_frame_buffer
 * Description    : read image data from FBUF 
 *                  
 * Input          : buffer_address(4 bytes); buffer_length(4 bytes)   ******YO!*******
 *                  
 * Output         : None
 * Return         : None
 *******************************************************************************/
void VC0706_read_frame_buffer(unsigned long buffer_address, unsigned long buffer_length)
{

	tx_vcbuffer[0]=VC0706_PROTOCOL_SIGN;
	tx_vcbuffer[1]=VC0706_SERIAL_NUMBER;
	tx_vcbuffer[2]=VC0706_COMMAND_READ_FBUF;
	tx_vcbuffer[3]=0x0c;
	tx_vcbuffer[4]=FBUF_CURRENT_FRAME;
	tx_vcbuffer[5]=0x0a;		// 0x0a=data transfer by MCU mode; 0x0f=data transfer by SPI interface
	tx_vcbuffer[6]=buffer_address>>24;			//starting address
	tx_vcbuffer[7]=buffer_address>>16;			
	tx_vcbuffer[8]=buffer_address>>8;			
	tx_vcbuffer[9]=buffer_address&0x0ff;			
	
	tx_vcbuffer[10]=buffer_length>>24;		// data length
	tx_vcbuffer[11]=buffer_length>>16;
	tx_vcbuffer[12]=buffer_length>>8;		
	tx_vcbuffer[13]=buffer_length&0x0ff;
	tx_vcbuffer[14]=0x00;		// delay time
	tx_vcbuffer[15]=0x0a;
	
	
	tx_counter=16;

	buffer_send();
}



/*******************************************************************************
 * Function Name  : VC0706_frame_control
 * Description    : control frame buffer register   ******YO!*******
 *                  
 * Input          : frame_control=control flag(1byte)
 *			: 		0 = stop current frame ; 1= stop next frame;2=step frame;3 =resume frame;
 *                  
 * Output         : None
 * Return         : None
 *******************************************************************************/
void VC0706_frame_control(byte frame_control)
{
	if(frame_control>3)frame_control=3;
	tx_vcbuffer[0]=VC0706_PROTOCOL_SIGN;
	tx_vcbuffer[1]=VC0706_SERIAL_NUMBER;
	tx_vcbuffer[2]=VC0706_COMMAND_FBUF_CTRL;
	tx_vcbuffer[3]=0x01;
	tx_vcbuffer[4]=frame_control;
	tx_counter=5;

	buffer_send();
}


/*******************************************************************************
 * Function Name  : VC0706_get_framebuffer_length
 * Description    : get byte-lengths in FBUF
 *                  
 * Input          : fbuf_type =current or next frame
 *			            0   =  current frame   ******YO!*******
 *				     1   =  next frame
 *                  
 * Output         : None
 * Return         : None
 *******************************************************************************/
void VC0706_get_framebuffer_length(byte fbuf_type)
{
	if(fbuf_type>1)fbuf_type=1;
	tx_vcbuffer[0]=VC0706_PROTOCOL_SIGN;
	tx_vcbuffer[1]=VC0706_SERIAL_NUMBER;
	tx_vcbuffer[2]=VC0706_COMMAND_GET_FBUF_LEN;
	tx_vcbuffer[3]=0x01;
	tx_vcbuffer[4]=fbuf_type;
	tx_counter=5;

	buffer_send();
}

/*******************************************************************************
 * Function Name  : VC0706_uart_power_save
 * Description    : stop current frame for reading
 *                  
 * Input          : power_on =1  start power-save
 *					   = 0  stop power-save
 *                  
 * Output         : None
 * Return         : None
 *******************************************************************************/
void VC0706_uart_power_save(byte power_save_on)
{
	tx_vcbuffer[0]=VC0706_PROTOCOL_SIGN;
	tx_vcbuffer[1]=VC0706_SERIAL_NUMBER;
	tx_vcbuffer[2]=VC0706_COMMAND_POWER_SAVE_CTRL;
	tx_vcbuffer[3]=0x03;
	tx_vcbuffer[4]=00;			//power save control mode
	tx_vcbuffer[5]=01;			// control by UART
	tx_vcbuffer[6]=power_save_on;			//start power save
	tx_counter=7;

	buffer_send();
}

/*******************************************************************************
 * Function Name  : VC0706_compression_ratio
 * Description	  : stop current frame for reading
 *					
 * Input		  : ration		>13(minimum)   ******YO!*******
 *						<63(max)
 *					
 * Output		  : None
 * Return		  : None
 *******************************************************************************/
void VC0706_compression_ratio(int ratio)
{
	if(ratio>63)ratio=63;
	if(ratio<13)ratio=13;
	int vc_comp_ratio=(ratio-13)*4+53;
	tx_vcbuffer[0]=VC0706_PROTOCOL_SIGN;
	tx_vcbuffer[1]=VC0706_SERIAL_NUMBER;
	tx_vcbuffer[2]=VC0706_COMMAND_WRITE_DATA;
	tx_vcbuffer[3]=0x05;
	tx_vcbuffer[4]=01;		//chip register
	tx_vcbuffer[5]=0x01;	//data num ready to write
	tx_vcbuffer[6]=0x12;	//register address
	tx_vcbuffer[7]=0x04;
	tx_vcbuffer[8]=vc_comp_ratio; //data

	tx_counter=9;

	buffer_send();
}
	


/*******************************************************************************
 * Function Name  : buffer_send
 * Description    : Transmit buffer to VC0706
 *                  
 * Input          : tx_vcbuffer
 *                  
 * Output         : None
 * Return         : None
 *******************************************************************************/
void buffer_send()
{
	int i=0;

	for (i=0;i<tx_counter;i++)
		Serial.write(tx_vcbuffer[i]);

	tx_ready=true;
}



/*******************************************************************************
 * Function Name  : buffer_read
 * Description    : Receive buffer from VC0706
 *                  
 * Input          : None
 *                  
 * Output         : rx_buffer, rx_ready
 * Return         : None
 *******************************************************************************/
void buffer_read()
{
	bool validity=true;

	if (rx_ready)			// if something unread in buffer, just quit
		return;

	rx_counter=0;
	VC0706_rx_buffer[0]=0;
	while (Serial.available() > 0) 
	{
		VC0706_rx_buffer[rx_counter++]= Serial.read();
		//delay(1);
	}

	if (VC0706_rx_buffer[0]!=0x76)
		validity=false;
	if (VC0706_rx_buffer[1]!=VC0706_SERIAL_NUMBER)
		validity=false;

	if (validity) rx_ready=true;


}


