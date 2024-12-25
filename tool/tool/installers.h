#pragma once

#include <3rdparty/WinReg.hpp>
#include <ulib/string.h>

namespace vcwin
{
    struct WindowsComponent
    {
        ulib::string DisplayName;
        ulib::string DisplayVersion;
        ulib::string UninstallString;
        bool SystemComponent;
        ulib::string guid;
    };

    inline ulib::list<WindowsComponent> list_uninstall_components()
    {
        ulib::list<WindowsComponent> result;
        ulib::list<ulib::wstring> keyPaths = {// L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
                                              L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"};
        for (auto &keyPath : keyPaths)
        {
            winreg::RegKey unode;
            unode.Create(HKEY_LOCAL_MACHINE, keyPath, KEY_READ | KEY_WOW64_32KEY | KEY_ENUMERATE_SUB_KEYS);

            auto subKeys = unode.EnumSubKeys();

            for (auto &sk : subKeys)
            {
                winreg::RegKey sknode;
                sknode.Create(HKEY_LOCAL_MACHINE, keyPath + L"\\" + sk, KEY_READ | KEY_WOW64_32KEY);

                auto dn = sknode.TryGetStringValue(L"DisplayName");
                auto dv = sknode.TryGetStringValue(L"DisplayVersion");
                auto us = sknode.TryGetStringValue(L"UninstallString");
                auto sc = sknode.TryGetDwordValue(L"SystemComponent");

                if (dn && dv)
                {
                    WindowsComponent component;
                    component.DisplayName = ulib::u8(dn.GetValue());
                    component.DisplayVersion = ulib::u8(dv.GetValue());
                    component.UninstallString = us ? ulib::u8(us.GetValue()) : "";
                    component.SystemComponent = bool(sc ? sc.GetValue() : 0);
                    component.guid = ulib::u8(sk);

                    result.push_back(std::move(component));
                }

                sknode.Close();
            }

            unode.Close();
        }

        return result;
    }

    inline ulib::list<WindowsComponent> list_installer_components()
    {
        ulib::list<WindowsComponent> result;

        ulib::wstring path = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData";

        winreg::RegKey unode;
        unode.Create(HKEY_LOCAL_MACHINE, path, KEY_READ | KEY_ENUMERATE_SUB_KEYS);
        auto sids = unode.EnumSubKeys();
        unode.Close();

        for (auto &sid : sids)
        {
            auto products = path + L"\\" + sid + L"\\Products";

            winreg::RegKey snode;
            snode.Create(HKEY_LOCAL_MACHINE, products, KEY_READ | KEY_ENUMERATE_SUB_KEYS);
            auto guids = snode.EnumSubKeys();
            snode.Close();

            for (auto &guid : guids)
            {
                winreg::RegKey gnode;
                gnode.Create(HKEY_LOCAL_MACHINE, products + L"\\" + guid + L"\\InstallProperties",
                             KEY_READ | KEY_ENUMERATE_SUB_KEYS);

                try
                {
                    auto dn = gnode.TryGetStringValue(L"DisplayName");
                    auto dv = gnode.TryGetStringValue(L"DisplayVersion");
                    auto us = gnode.TryGetStringValue(L"UninstallString");
                    auto sc = gnode.TryGetDwordValue(L"SystemComponent");

                    if (dn && dv)
                    {
                        WindowsComponent component;
                        component.DisplayName = ulib::u8(dn.GetValue());
                        component.DisplayVersion = ulib::u8(dv.GetValue());
                        component.UninstallString = us ? ulib::u8(us.GetValue()) : "";
                        component.SystemComponent = bool(sc ? sc.GetValue() : 0);
                        component.guid = ulib::u8(guid);

                        result.push_back(std::move(component));
                    }
                }
                catch (const std::exception& ex)
                {
                    printf("ex: %s\n", ex.what());
                }

                gnode.Close();
            }
        }

        return result;
    }

} // namespace vcwin