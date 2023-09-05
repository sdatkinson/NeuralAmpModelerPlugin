#pragma once

#include "dirscan.h"
#include "wdlstring.h"
#include <functional>
#include <string_view>
#include <filesystem>

using CompletionHandlerFunc = std::function<void(const WDL_String&, const WDL_String&)>;

using namespace iplug::igraphics;

class FileManager
{
public:
  /** Creates a FileManager
   * @param bounds The control's bounds
   * @param extension The file extenstion to browse for, e.g excluding the dot e.g. "txt"
   * @param showFileExtension Should the menu show the file extension
   * @param scanRecursively Should the paths be scanned recursively, creating submenus
   * @param showEmptySubmenus If scanRecursively, should empty folders be shown in the menu */
  FileManager(const char* extension, bool showFileExtensions = true, bool scanRecursively = true, bool showEmptySubmenus = false)
  : mExtension(extension)
  , mShowFileExtensions(showFileExtensions)
  , mScanRecursively(scanRecursively)
  , mShowEmptySubmenus(showEmptySubmenus)
  {
    mMainMenu.SetRootTitle(std::string_view(extension) == "nam" ? "Models" : "IRs");
  }

  ~FileManager()
  {
    ClearItems();
  }
  
  IPopupMenu& GetMenu() { return mMainMenu; }
  
  void OnPicked(const WDL_String& fileName, const WDL_String& path, bool loadFile = false)
  {
    if (path.GetLength())
    {
      ClearItems();
      mPath.Set(path.Get());
      SetupMenu();
      fileName.GetLength() ? SelectFileWithPath(fileName.Get()) : SelectFirstFile();
      
      if (loadFile)
        LoadFileAtCurrentIndex();
    }
  }

  int NItems() const
  {
    return mItems.GetSize();
  }
  
  void SelectFileWithItem(IPopupMenu::Item* pItem)
  {
    mSelectedIndex = mItems.Find(pItem);
  }

  void SelectFirstFile() { mSelectedIndex = mFiles.GetSize() ? 0 : -1; }

  void SelectFileWithPath(const char* filePath)
  {
    for (auto fileIdx = 0; fileIdx < mFiles.GetSize(); fileIdx ++)
    {
      if (strcmp(mFiles.Get(fileIdx)->Get(), filePath) == 0)
      {
        for (auto itemIdx = 0; itemIdx < mItems.GetSize(); itemIdx++)
        {
          IPopupMenu::Item* pItem = mItems.Get(itemIdx);

          if (pItem->GetTag() == fileIdx)
          {
            mSelectedIndex = itemIdx;
            return;
          }
        }
      }
    }
    
    mSelectedIndex = -1;
  }

  void GetSelectedFile(WDL_String& path) const
  {
    if (mSelectedIndex > -1)
    {
      IPopupMenu::Item* pItem = mItems.Get(mSelectedIndex);
      path.Set(mFiles.Get(pItem->GetTag()));
    }
    else
    {
      path.Set("");
    }
  }

  const char* GetExtension() const
  {
    return mExtension.Get();
  }
  
  void CheckSelectedItem()
  {
    if (mSelectedIndex > -1)
    {
      mMainMenu.SetChosenItemIdx(mSelectedIndex);
      IPopupMenu::Item* pItem = mItems.Get(mSelectedIndex);
      mMainMenu.CheckItemAlone(pItem);
    }
  }

  void PreviousFile()
  {
    const auto nItems = NItems();
    if (nItems == 0)
      return;
    mSelectedIndex--;

    if (mSelectedIndex < 0)
      mSelectedIndex = nItems - 1;

    LoadFileAtCurrentIndex();
  }

  void NextFile()
  {
    const auto nItems = NItems();
    if (nItems == 0)
      return;
    mSelectedIndex++;

    if (mSelectedIndex >= nItems)
      mSelectedIndex = 0;

    LoadFileAtCurrentIndex();
  }
  
  void LoadFileAtCurrentIndex()
  {
    if (mSelectedIndex > -1 && mSelectedIndex < NItems())
    {
      WDL_String fileName, path;
      GetSelectedFile(fileName);
      mCompletionHandler(fileName, path);
    }
  }
  
  void SetCompletionHandler(CompletionHandlerFunc completionHandler) {
    mCompletionHandler = completionHandler;
  }
  
  void SetDefaultPath(const char* path) { mDefaultPath.Set(path); }
  const char* GetDefaultPath() const { return mDefaultPath.Get(); }
  
#pragma mark - Private methods:
  
private:

  void CollectSortedItems(IPopupMenu* pMenu)
  {
    int nItems = pMenu->NItems();
    
    for (int i = 0; i < nItems; i++)
    {
      IPopupMenu::Item* pItem = pMenu->GetItem(i);
      
      if (pItem->GetSubmenu())
        CollectSortedItems(pItem->GetSubmenu());
      else
        mItems.Add(pItem);
    }
  }

  void SetupMenu()
  {
    mFiles.Empty(true);
    mItems.Empty(false);
    
    mMainMenu.Clear();
    mSelectedIndex = -1;

    ScanDirectory(mPath.Get(), mMainMenu);
    CollectSortedItems(&mMainMenu);
  }

  void ClearItems()
  {
    mFiles.Empty(true);
    mItems.Empty(false);
  }

  void ScanDirectory(const char* path, IPopupMenu& menuToAddTo)
  {
    WDL_DirScan d;

    if (!d.First(path))
    {
      do
      {
        const char* f = d.GetCurrentFN();

        if (f)
        {
          std::filesystem::path path(f);

          if (f[0] != '.' && mScanRecursively && d.GetCurrentIsDirectory())
          {
            WDL_String subdir;
            d.GetCurrentFullFN(&subdir);
            IPopupMenu* pNewMenu = new IPopupMenu();
            menuToAddTo.AddItem(d.GetCurrentFN(), pNewMenu, -2);
            ScanDirectory(subdir.Get(), *pNewMenu);
          }
          else
          {
            auto extensionWithDot = std::string(".") + std::string(mExtension.Get());

            if (path.extension() == extensionWithDot || path.extension() == ".icloud")
            {
              if (path.extension() == ".icloud")
              {
                auto newPath = path.replace_extension("");
                
                if (newPath.extension() == extensionWithDot)
                {
                  path = newPath.replace_filename(newPath.filename().string().substr(1));
                }
                else
                {
                  break;
                }
              }

              {
                WDL_String menuEntry {path.c_str()};
                
                if (!mShowFileExtensions)
                  menuEntry.remove_fileext();
                
                IPopupMenu::Item* pItem = new IPopupMenu::Item(menuEntry.Get(), IPopupMenu::Item::kNoFlags, mFiles.GetSize());
                menuToAddTo.AddItem(pItem, -2 /* sort alphabetically */);
                WDL_String* pFullPath = new WDL_String("");
                d.GetCurrentFullFN(pFullPath);
                mFiles.Add(pFullPath);
              }
            }
          }
        }
      } while (!d.Next());
    }

    if (!mShowEmptySubmenus)
      menuToAddTo.RemoveEmptySubmenus();
  }

private:
  CompletionHandlerFunc mCompletionHandler;
  const bool mScanRecursively;
  const bool mShowFileExtensions;
  const bool mShowEmptySubmenus;
  int mSelectedIndex = -1;
  IPopupMenu mMainMenu;
  WDL_String mPath;
  WDL_PtrList<WDL_String> mFiles;
  WDL_PtrList<IPopupMenu::Item> mItems; // ptr to item for each file
  WDL_String mExtension;
  WDL_String mDefaultPath;
};
