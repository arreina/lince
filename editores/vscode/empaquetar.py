#!/usr/bin/env python3
"""Empaqueta la extensión de VS Code en un .vsix sin necesitar 'vsce' ni npm.

Un .vsix es un zip con la extensión dentro de 'extension/' más dos ficheros de
metadatos. Se instala con:  code --install-extension lince-language-X.Y.Z.vsix

Uso: python3 editores/vscode/empaquetar.py [carpeta-de-salida]
"""
import json, pathlib, sys, zipfile
from xml.sax.saxutils import escape

aqui = pathlib.Path(__file__).resolve().parent
ext = aqui / "lince-language"
salida = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else aqui
pkg = json.loads((ext / "package.json").read_text(encoding="utf-8"))

nombre, version = pkg["name"], pkg["version"]
destino = salida / f"{nombre}-{version}.vsix"

manifiesto = f"""<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011">
  <Metadata>
    <Identity Language="es" Id="{escape(nombre)}" Version="{escape(version)}" Publisher="{escape(pkg['publisher'])}"/>
    <DisplayName>{escape(pkg['displayName'])}</DisplayName>
    <Description xml:space="preserve">{escape(pkg['description'])}</Description>
    <Tags>lince,español</Tags>
    <Categories>Programming Languages</Categories>
    <GalleryFlags>Public</GalleryFlags>
    <Properties>
      <Property Id="Microsoft.VisualStudio.Code.Engine" Value="{escape(pkg['engines']['vscode'])}"/>
    </Properties>
    <Icon>extension/icon.png</Icon>
  </Metadata>
  <Installation><InstallationTarget Id="Microsoft.VisualStudio.Code"/></Installation>
  <Dependencies/>
  <Assets>
    <Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true"/>
    <Asset Type="Microsoft.VisualStudio.Services.Icons.Default" Path="extension/icon.png" Addressable="true"/>
  </Assets>
</PackageManifest>
"""

tipos = """<?xml version="1.0" encoding="utf-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/Content-Types">
  <Default Extension=".json" ContentType="application/json"/>
  <Default Extension=".js" ContentType="application/javascript"/>
  <Default Extension=".png" ContentType="image/png"/>
  <Default Extension=".vsixmanifest" ContentType="text/xml"/>
</Types>
"""

with zipfile.ZipFile(destino, "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("[Content_Types].xml", tipos)
    z.writestr("extension.vsixmanifest", manifiesto)
    for f in sorted(ext.rglob("*")):
        if f.is_file():
            z.write(f, "extension/" + f.relative_to(ext).as_posix())
print(f"✅ {destino}")
