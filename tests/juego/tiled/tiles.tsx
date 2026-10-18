<?xml version="1.0" encoding="UTF-8"?>
<tileset version="1.10" tiledversion="1.11.0" name="tiles" tilewidth="8" tileheight="8" tilecount="8" columns="4">
 <image source="tiles.png" width="32" height="16"/>
 <tile id="0">
  <properties>
   <property name="solido" type="bool" value="true"/>
  </properties>
 </tile>
 <tile id="2">
  <properties>
   <property name="plataforma" type="bool" value="true"/>
  </properties>
 </tile>
 <tile id="3">
  <animation>
   <frame tileid="3" duration="100"/>
   <frame tileid="1" duration="100"/>
  </animation>
 </tile>
</tileset>
