<?xml version="1.0" encoding="utf-8" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
		xmlns:am="http://www.digicine.com/PROTO-ASDCP-AM-20040311#"
		xmlns:pl="http://www.digicine.com/PROTO-ASDCP-PKL-20040311#"
		xmlns:cpl="http://www.digicine.com/PROTO-ASDCP-CPL-20040511#">
  <xsl:output method="xml" encoding="utf-8"/>
  <xsl:variable name="asset_map" select="/am:AssetMap/am:AssetList"/>
  <xsl:template match="/am:AssetMap">
    <Subtitles>
      <xsl:text>&#10;</xsl:text>
      <xsl:for-each select="am:AssetList">


      </xsl:for-each>
      <xsl:for-each select="am:AssetList/am:Asset[am:PackingList]/am:ChunkList/am:Chunk">
	<xsl:apply-templates select="document(am:Path, .)"/>
      </xsl:for-each>
    </Subtitles>
  </xsl:template>
  
  <xsl:template match="/pl:PackingList">
    <!-- Find CPL -->
    <xsl:for-each select="pl:AssetList/pl:Asset[pl:Type = 'text/xml;asdcpKind=CPL']"> 
      <xsl:variable name="urn" select="pl:Id"/>
      <xsl:variable name="path" select="$asset_map/am:Asset[am:Id = $urn]/am:ChunkList/am:Chunk/am:Path"/>
      <xsl:apply-templates select="document($path)"/>
    </xsl:for-each>
  </xsl:template>

  <!-- Process reels -->
  <xsl:template match="cpl:CompositionPlaylist">
    <xsl:for-each select="cpl:ReelList/cpl:Reel">
      <xsl:variable name="offset">
	<xsl:variable name="edit_rate_str" select="cpl:AssetList/cpl:MainSubtitle/cpl:EditRate"/>
	<xsl:variable name="edit_rate" select="substring-before($edit_rate_str, ' ') div substring-after($edit_rate_str, ' ')"/>
	<xsl:value-of select="sum(preceding-sibling::cpl:Reel/cpl:AssetList/cpl:MainSubtitle/cpl:Duration) * 1000 div $edit_rate"/>
      </xsl:variable>

      <xsl:for-each select="cpl:AssetList/cpl:MainSubtitle">
	<xsl:variable name="urn" select="cpl:Id"/>
	<xsl:variable name="path" select="$asset_map/am:Asset[am:Id = $urn]/am:ChunkList/am:Chunk/am:Path"/>
	
	<xsl:apply-templates select="document($path, $asset_map)">
	  
	  <xsl:with-param name="LTC-offset" select="$offset"/>
	</xsl:apply-templates>
      </xsl:for-each>
    </xsl:for-each>

  </xsl:template>
  
  <!-- Process spots -->
  <xsl:template match="DCSubtitle">
    <xsl:param name="LTC-offset" select="0"/>
    <xsl:param name="reel"/>
    <xsl:for-each select="Font/Subtitle">
      <Subtitle>
	<xsl:variable name="in" select="@TimeIn"/>
	<xsl:variable name="inms" select="(number(substring($in,1,2))*3600+number(substring($in,4,2))*60+number(substring($in,7,2)))*1000+number(substring($in,10,3))*4"/>
	<xsl:variable name="out" select="@TimeOut"/>
	<xsl:variable name="outms" select="(number(substring($out,1,2))*3600+number(substring($out,4,2))*60+number(substring($out,7,2)))*1000+number(substring($out,10,3))*4"/>
	
	<!-- Reel -->
	<xsl:attribute name="Reel">
	  <xsl:value-of select="ancestor::DCSubtitle/ReelNumber"/>
	</xsl:attribute>
	<!-- SpotNumber -->
	<xsl:attribute name="SpotNumber">
	  <xsl:value-of select="@SpotNumber"/>
	</xsl:attribute>
	<!-- Reel times -->
	<xsl:attribute name="TimeIn">
	  <xsl:call-template name="format-ms">
	    <xsl:with-param name="ms" select="number($inms)"/>
	  </xsl:call-template>
	</xsl:attribute>
	<xsl:attribute name="TimeOut">
	  <xsl:call-template name="format-ms">
	    <xsl:with-param name="ms" select="number($outms)"/>
	  </xsl:call-template>
	</xsl:attribute>

	<!-- CPL times -->
	<xsl:attribute name="LTCIn">
	  <xsl:call-template name="format-ms">
	    <xsl:with-param name="ms" select="number($inms+$LTC-offset)"/>
	  </xsl:call-template>
	</xsl:attribute>
	<xsl:attribute name="LTCOut">
	  <xsl:call-template name="format-ms">
	    <xsl:with-param name="ms" select="number($outms+$LTC-offset)"/>
	  </xsl:call-template>
	</xsl:attribute>
	<xsl:text>&#10;</xsl:text>
	<xsl:for-each select="Text">
	  <Row>
	    <xsl:value-of select="."/>
	    </Row><xsl:text>&#10;</xsl:text>
	</xsl:for-each>
	</Subtitle><xsl:text>&#10;</xsl:text>
    </xsl:for-each>
  </xsl:template>
  
  <xsl:template match="@*|node()|comment()">
  </xsl:template>

  <xsl:template name="format-ms">
    <xsl:param name="ms"/>
    <xsl:value-of select="format-number(floor($ms div 3600000) mod 24, '00')"/>
    <xsl:text>:</xsl:text>
    <xsl:value-of select="format-number(floor($ms div 60000) mod 60, '00')"/>
    <xsl:text>:</xsl:text>
    <xsl:value-of select="format-number(floor($ms div 1000) mod 60,'00')"/>
    <xsl:text>.</xsl:text>
    <xsl:value-of select="format-number($ms mod 1000,'000')"/>
  </xsl:template>

</xsl:stylesheet>
