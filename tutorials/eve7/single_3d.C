/*
 * Pre-script for running with a single 3D element on the client.
 * Run as, e.g., root.exe single_3d.C jets.C
 * Also see some helper files in ui5/eve7/.... to be figured out.
*/

void single_3d()
{
   namespace REX = ROOT::Experimental;

   auto eve_mgr = REX::REveManager::Create();
   eve_mgr->AllowMultipleRemoteConnections(false, false);

   /* // From fireworks web
      const char* mypath =  Form("%s/src/FireworksWeb/Core/ui5/",gSystem->Getenv("CMSSW_BASE"));
      // printf("--- mypath ------ [%s] \n", mypath);
      std::string fp = "fireworks-" + ROOT::Experimental::gEve->GetWebWindow()->GetClientVersion()+"/";
      ROOT::Experimental::gEve->AddLocation(fp,  mypath);
      std::string dp = (Form("file:%s/fireworks.html", fp.c_str()));
      ROOT::Experimental::gEve->SetDefaultHtmlPage(dp);
    */

   //const char *ui5p = gEnv->GetValue("WebGui.RootUi5Path", gSystem->ExpandPathName("$ROOTSYS/ui5"));
   eve_mgr->SetDefaultHtmlPage("file:/rootui5sys/eve7/index-mini.html");
}
