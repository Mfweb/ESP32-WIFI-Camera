using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Net.Sockets;
using System.Net;
using System.Drawing.Imaging;
using System.Threading;

namespace video_test
{
    public partial class Form1 : Form
    {
        UdpClient udp_ct;
        byte[] fHead = { 0x55, 0xaa, 0x55, 0xaa, 0xa0 };
        byte[] fEnd  = { 0xaa, 0x55, 0xaa, 0x55, 0xaf };
        int f_count = 0;
        float f_speed = 0;
        SynchronizationContext img_sync = null;

        Thread rev_th = null;
        public Form1()
        {
            InitializeComponent();
            img_sync = SynchronizationContext.Current;
        }

        private void set_img(object image)
        {
            pictureBox1.Image = (Bitmap)image;
            pictureBox1.Update();
        }

        unsafe private void udp_handle()
        {
            int count = 0;
            udp_ct = new UdpClient();
            Bitmap temp_bitmap = new Bitmap(320, 240);
            udp_ct.Connect("192.168.4.1", 4567);
            Byte[] sendBytes = Encoding.ASCII.GetBytes("a");
            IPEndPoint RemoteIpEndPoint = new IPEndPoint(IPAddress.Parse("192.168.4.1"), 4567);
            udp_ct.Send(sendBytes, sendBytes.Length);
            int mode = 0;
            bool fist_head = false;
            BitmapData tdata = null;
            UInt16* p = null;
            while (true)
            {
                Byte[] receiveBytes = udp_ct.Receive(ref RemoteIpEndPoint);//等待接收一帧数据
                f_speed += receiveBytes.Length;
                int tcount = 0;
                bool find = true;

                for (int i = 0; i < receiveBytes.Length; i++)
                {
                    if (receiveBytes[i] != fHead[tcount++])
                    {
                        find = false;
                        break;
                    }
                    if (tcount >= 5) tcount = 0;
                }
                if (find)
                {
                    mode = 1;//找到了头
                    fist_head = true;
                    if (tdata == null)
                    {
                        tdata = temp_bitmap.LockBits(new Rectangle(0, 0, temp_bitmap.Width, temp_bitmap.Height), ImageLockMode.ReadWrite, PixelFormat.Format16bppRgb565);
                    }
                    p = (UInt16*)tdata.Scan0;
                }
                else
                {
                    find = true;
                    tcount = 0;
                    for (int i = 0; i < receiveBytes.Length; i++)
                    {
                        if (receiveBytes[i] != fEnd[tcount++])
                        {
                            find = false;
                            break;
                        }
                        if (tcount >= 5) tcount = 0;
                    }
                    if (find) mode = 2;//找到了尾
                }

                if (mode == 1 && fist_head == false)//找到了头开始记录数据
                {
                    if (count + receiveBytes.Length > 240 * 320 * 2)
                    {
                        label1.Text = (count + receiveBytes.Length).ToString();
                        count = 0;
                        mode = 0;
                        temp_bitmap.UnlockBits(tdata);
                        tdata = null;
                        p = null;
                    }
                    else
                    {
                        for (int i = 0; i < receiveBytes.Length; i+=2)//直接转到内存中
                        {
                            if (p != null)
                            {
                                *(p + (count >> 1) + (i >> 1)) = (UInt16)((receiveBytes[i] << 8) | receiveBytes[i + 1]);
                            }
                        }
                        count += receiveBytes.Length;
                    }

                }
                else if (mode == 2)//找到了尾开始显示数据
                {
                    if (tdata != null)
                    {
                        temp_bitmap.UnlockBits(tdata);
                        Bitmap n_img;
                        n_img = (Bitmap)temp_bitmap.Clone();
                        img_sync.Post(set_img, n_img);
                        tdata = null;
                        p = null;
                        f_count++;
                    }

                    count = 0;
                    mode = 0;
                }
                Application.DoEvents();
                if (mode == 1) fist_head = false;
            }
            //MessageBox.Show(receiveBytes.Length.ToString());
        }
        private void button1_Click(object sender, EventArgs e)
        {
            if (button1.Text == "start")
            {
                rev_th = new Thread(udp_handle);
                rev_th.IsBackground = true;
                rev_th.Start();
                button1.Text = "stop";
            }
            else
            {
                rev_th.Abort();
                if (udp_ct != null)
                {
                    udp_ct.Close();
                    udp_ct.Dispose();
                }
                button1.Text = "start";
            }
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            
            
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            label2.Text = f_count.ToString() + "FPS";
            f_count = 0;
            label3.Text = (f_speed / 1024).ToString() + "KB/S";
            f_speed = 0;
        }
    }
}
