import zipfile, os, fileinput, string, sys, shutil

scriptpath = os.path.dirname(os.path.realpath(__file__))
projectpath = os.path.abspath(os.path.join(scriptpath, os.pardir))

IPLUG2_ROOT = "..\..\iPlug2"

sys.path.insert(0, os.path.join(scriptpath, IPLUG2_ROOT + '\Scripts'))

from get_archive_name import get_archive_name

def main():
  if len(sys.argv) != 3:
    print("Usage: make_zip.py demo[0/1] zip[0/1]")
    sys.exit(1)
  else:
    demo=int(sys.argv[1])
    zip=int(sys.argv[2])

  dir = projectpath + "\\build-win\\out"
  
  if os.path.exists(dir):
    shutil.rmtree(dir)
  
  os.makedirs(dir)
  
  files = []

  if not zip:
    installer = "\\build-win\\installer\\TemplateProject Installer.exe"
    
    if demo:
      installer = "\\build-win\\installer\\TemplateProject Demo Installer.exe"
    
    files = [
      projectpath + installer,
      projectpath + "\\installer\\changelog.txt",
      projectpath + "\\installer\\known-issues.txt",
      projectpath + "\\manual\\TemplateProject manual.pdf" 
    ]
  else:
    files = [
      projectpath + "\\build-win\\TemplateProject.vst3\\Contents\\x86_64-win\\TemplateProject.vst3",
      projectpath + "\\build-win\\TemplateProject_x64.exe"  
    ]

  zipname = get_archive_name(projectpath, "win", "demo" if demo == 1 else "full" )

  zf = zipfile.ZipFile(projectpath + "\\build-win\\out\\" + zipname + ".zip", mode="w")

  for f in files:
    print("adding " + f)
    zf.write(f, os.path.basename(f), zipfile.ZIP_DEFLATED)

  zf.close()
  print("wrote " + zipname)

  zf = zipfile.ZipFile(projectpath + "\\build-win\\out\\" + zipname + "-pdbs.zip", mode="w")

  files = [
    projectpath + "\\build-win\\pdbs\\TemplateProject-vst3_x64.pdb",
    projectpath + "\\build-win\\pdbs\\TemplateProject-app_x64.pdb"  
  ]

  for f in files:
    print("adding " + f)
    zf.write(f, os.path.basename(f), zipfile.ZIP_DEFLATED)

  zf.close()
  print("wrote " + zipname)

if __name__ == '__main__':
  main()
