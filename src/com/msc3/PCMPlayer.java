package com.msc3;

import java.io.BufferedOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.net.UnknownHostException;
import java.util.ArrayList;




import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.os.Environment;
import android.util.Log;
import android.view.View;
import android.widget.ImageView;

public class PCMPlayer implements Runnable {
	
	public static final int BUFFER_EMPTY = 0;
	public static final int BUFFER_PROCESSING = 1;
	public static final int BUFFER_FULL = 2;
	
	
	private boolean _isAlive ;
	private ArrayList<byte[]> _buffers ;
	private AudioTrack _at;
	
	private static final int NUM_BUFF = 16; 
	private static final int ONE_BUFF_SIZE = 8*1010; 
	
	private byte s_buffers [][] ;
	private int s_buffers_len [] ; //Store len of the s_buffers, it can be < 8080
	private int s_buffers_offset[]; //store the offset to continue to read 
	
	private int s_buffer_status[] ; //status of buffer EMPTY , INPROCESS,  FULL
	private int write_next, read_next ;
	private int minBufferSize;
	private int maxBufferSize; 
	private int overallDataBuffered; 
	private String filename;
	private FileOutputStream os ;
	
	private int pcmFreq; 
	private int numChannels; 
	
	private boolean debug_write_to_file = true;
	
	
	public PCMPlayer(int freq, int num_channels) {
		_isAlive =true;
		_buffers = new ArrayList<byte[]>();
		
		/// new way - hardcode buffer
		s_buffers = new byte[NUM_BUFF][ONE_BUFF_SIZE];
		s_buffers_len = new int [NUM_BUFF];
		s_buffers_offset = new int [NUM_BUFF];
		s_buffer_status = new int [NUM_BUFF];
		read_next = 0;
		write_next = 0;
		
		if (debug_write_to_file)
		{
			String path = Environment.getExternalStorageDirectory().getPath() + File.separator;
			filename = path + "test.pcm";
			Log.d("mbp", "AUDIO data is " + filename); 
		}
		
		
		for (int i =0; i< NUM_BUFF; i++)
		{
			s_buffer_status[i] = BUFFER_EMPTY;
			s_buffers_offset[i] = 0; 
		}
		
		pcmFreq =  freq; 
		numChannels = num_channels;
	}	
	
	
	public void run() {
		
		int overallData = 0;
		
		boolean startedPlaying = false;
		
		minBufferSize = AudioTrack.getMinBufferSize(pcmFreq, 
				numChannels, 
				AudioFormat.ENCODING_PCM_16BIT);
		//ihomephone: minBufferSize ~ 2k 
		
		//OK CODE: maxBufferSize = minBufferSize *6;
		/*also ok maxBufferSize = minBufferSize *4;
		
		if (maxBufferSize < ONE_BUFF_SIZE)
		{
			maxBufferSize =ONE_BUFF_SIZE;
		}*/
		
		maxBufferSize = minBufferSize *2;
		if ( minBufferSize >= 2972) //iHOME phone
		{
			maxBufferSize = minBufferSize; 
		}
		_at = new AudioTrack(AudioManager.STREAM_MUSIC, 
				pcmFreq, 
				numChannels, 
				AudioFormat.ENCODING_PCM_16BIT, 
				maxBufferSize, 
				AudioTrack.MODE_STREAM);

		if (debug_write_to_file)
		{
			initWriteTofile();
		}
		

		/*Log.d("mbp", "minBufferSize: " + minBufferSize+ 
				" maxBuff:" + maxBufferSize); */

		overallDataBuffered = 0; 
		int written; 
		int toBeWritten; 
		while(_isAlive) {
			written = 0; 

			if (s_buffer_status[read_next] != BUFFER_FULL)
			{
				//Log.d("mbp","Buffer empty ..read_next:" + read_next +
					//	"write_next:" + write_next); 
				try {
					Thread.sleep(20);
				} catch (InterruptedException ie) {
					break;
				}
			}
			else
			{
				
				s_buffer_status[read_next] = BUFFER_PROCESSING;
				//synchronized(s_buffers)
				{
					
					/*  */
					if (startedPlaying)
					{

						if (shouldMuteTrackNow(s_buffers[read_next],s_buffers_len[read_next]))
						{
							if (_at.getPlayState() != AudioTrack.PLAYSTATE_PAUSED)
							{
								_at.pause();
							}
						}
						else
						{
							if (_at.getPlayState() == AudioTrack.PLAYSTATE_PAUSED)
							{
								_at.flush();//flush old data to avoid delay
								_at.play();

							}
						}
					}


					if (s_buffers_len[read_next] >0)
					{
						/* IMPORTANT NOTE: 
						 *  write () will not really write any thing IF the 
						 *  audiotrack internal buffer(set when created above) 
						 *  is full and it's not been played (play()) not called
						 *
						 *  @written: is the returned number of bytes written - 
						 *   it could be SMALLER than the the actual s_buffers_len[read_next]
						 *   NEED to manage this to make sure data is written completely.
						 */
						toBeWritten = s_buffers_len[read_next] - s_buffers_offset[read_next];
						//Log.d("mbp","befor write: " + toBeWritten + " offset: " + s_buffers_offset[read_next] ); 
						written = _at.write(s_buffers[read_next], s_buffers_offset[read_next],toBeWritten);
					
						if ( (written >= toBeWritten)||
								(_at.getPlayState() == AudioTrack.PLAYSTATE_PAUSED) ||
								(_at.getPlayState() == AudioTrack.PLAYSTATE_STOPPED)
							)
						{
							//finish writing , or incase of pause, discard this audio buff 
							
							s_buffers_len[read_next] = 0; 
							s_buffers_offset[read_next] = 0; 
							s_buffer_status[read_next] = BUFFER_EMPTY;
							read_next = (read_next+1)%NUM_BUFF;
						}
						else
						{
							Log.d("mbp","written: " + written
									+ " tobewritten" + toBeWritten 
									+ " dataLen: " + s_buffers_len[read_next]
									 + " playState: " + _at.getPlayState()
									 + " head: " + _at.getPlaybackHeadPosition()
									); 
							//remember the position where we read until 
							s_buffers_offset[read_next] += written; 
							//force this buffer to FULL again 
							s_buffer_status[read_next] = BUFFER_FULL;
							//dont increment read_next 
						}
						
						overallData +=written ; 

						
						/* We start to play only AFTER we write more than 
						 *1sec of data (16000) //minBufferSize of data 
						 */
						if ( (overallData >=minBufferSize) && 
								(overallDataBuffered > 16000 ) && 
								(startedPlaying ==false))
						{
							//Log.d("mbp", "start play at: " +overallData ); 
							_at.play();
							startedPlaying = true;
						}
//						else if  (overallData >=minBufferSize)
//						{
//							Log.d("mbp", "overallData playable at: " +overallData );
//						}

					}
					else
					{
						s_buffers_offset[read_next] = 0; 
						s_buffer_status[read_next] = BUFFER_EMPTY;
						read_next = (read_next+1)%NUM_BUFF;

					}

				}//synch block
			}

		}

		_at.flush();
		_at.stop();
		_at.release();
	}

	/* 20120407: check for db level over 100ms block */ 
	/**
	 * simple logic: if in any block.. we find that the db level raises higher than THRESHOLD
	 *                  we don't mute -- actual sound is coming.. 
	 *               if in all block.. we find that the db level lower than the THRESHOLD
	 *                  we mute
	 * 
	 * @param pcm_data - 1sec worth of data 
	 * @return
	 */
	private static final double DB_CUTOFF_LVL = -65; 
	private boolean shouldMuteTrackNow(byte [] pcm_data, int pcm_len)
	{
		double level,  average_sample = 0 , total_sample= 0; 
		short sample16bit = 0; 
		
		for (int i=0; i<pcm_len; i+=2 )
		{
			sample16bit =  (short) (pcm_data[i+1] & 0xFF);
			sample16bit = (short) (sample16bit<<8) ;
			sample16bit += pcm_data[i]& 0xFF;
			
			total_sample +=  Math.abs(sample16bit) ; 
		}
		
		average_sample = total_sample/(pcm_len/2);
		level = 20* Math.log10(Math.abs(average_sample) / 32768);
//		Log.d("mbp", "audio len: " + pcm_len);
//		Log.d("mbp", "for  avg lve: "+ level);
		if (level > DB_CUTOFF_LVL)
		{
			return false; 
		}
		
		return true; 
	}
	
	
	
	
	
	/**** store the offset of each sub-buffer */
	public void writePCM(byte[] pcm, int pcm_len) {
		int offset = 0; 

		while(offset < pcm_len)
		{
			int copied_len;

			if( (offset + ONE_BUFF_SIZE) <= pcm_len)
			{
				copied_len = ONE_BUFF_SIZE;
//				Log.d("mbp","PCMPlayer spill over nxt buff pcmlen: " + pcm_len + 
//						" offset: "+ offset); 
			}
			else
			{
				copied_len = pcm_len - offset;
			}


			if(s_buffer_status[write_next] == BUFFER_EMPTY)
			{
				s_buffer_status[write_next] = BUFFER_PROCESSING;
				//synchronized(s_buffers) 
				{
					System.arraycopy(pcm, offset, s_buffers[write_next], 0,copied_len );
					s_buffers_len[write_next] = copied_len;
					s_buffers_offset[write_next] = 0;//start reading at 0 
					s_buffer_status[write_next] = BUFFER_FULL;

					overallDataBuffered += copied_len;
							
					offset += copied_len;
					write_next = (write_next +1 )%NUM_BUFF;
					
					
				}
				
			}
			else// s_buffer_status[write_next] != PublicDefine.BUFFER_EMPTY
			{ 
//				Log.d("mbp","Buffer busy -- sleep for a 50" +
//						"write_next= " +write_next +" read_next=" + read_next);
				//wait fot the buffer to be consumed.
				try {
					Thread.sleep(63);
				} catch (InterruptedException e) {
				}
				break;
			}
			
		}

		
		

		if (debug_write_to_file ==true)
		{
			//DBG write pcm data to file
			writeAudioDataToFile(pcm,pcm_len);
		}
	}
	
	
	
	public void stop() {
		_isAlive = false;
		
		if (debug_write_to_file)
		{
			stopWriteTofile();
		}
		
	}


	private void initWriteTofile()
	{
		Log.d("mbp","pcm file is: " + filename);
		os = null;
		try {
			os = new FileOutputStream(filename);
		} catch (FileNotFoundException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	private void stopWriteTofile()
	{
		if (os != null)
		{
			try {
				os.close();
			} catch (IOException e) {
				e.printStackTrace();
			}
		}
	}
	
	private void writeAudioDataToFile(byte[] pcm, int pcm_len){
		
		if (os == null)
		{
			Log.e("mbp", "ERROR NO OUTPUT STREAM");
			return; 
		}
		int read = 0;

		try {
			os.write(pcm, 0, pcm_len);
		} catch (IOException e) {
			Log.e("mbp", "ERROR WHILE WRITING");
			os = null;
		}

	}
}
