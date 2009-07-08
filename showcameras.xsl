<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:output method = "html" indent = "yes" />
<xsl:template match="/">


<html xsl:version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns="http://www.w3.org/1999/xhtml">
	<head>
		<title>Known RawSpeed Cameras</title>
		<style type="text/css">
			body {
				font-family:Verdana;font-size:12pt;background-color:#ffffff;
			}
			h1 {
				font-size:16pt;padding:8px;
			}
			h2 {
				background-color:teal;color:white;padding:8px;font-size:13pt;
			}
			span.param {
				font-style:italic;color:#666;
			}
			div.text {
				margin-left:20px;margin-bottom:15px;font-size:10pt;margin-top:0px;line-height:120%;
			}
		</style>
	</head>
	
  <body>
		<h1>Known RawSpeed Cameras</h1>
    <xsl:for-each select="Cameras/Camera">
		<xsl:sort data-type = "text" select = "concat(@make,@model)"/>
      <h2>
        <span style="font-weight:bold"><xsl:value-of select="@make"/></span>
        - <xsl:value-of select="@model"/>
      </h2>
      <div class="text">
        <xsl:variable name = "supported" ><xsl:value-of select="@supported"/></xsl:variable>
         <xsl:if test ="$supported = 'no'">Supported:<span style="color:red;font-style:italic;">No.</span></xsl:if>
         <xsl:if test ="not($supported = 'no')">Supported:<span style="color:green;font-style:italic;">Yes.</span>
				 <br/>
				 Cropped Image Size: <span class="param"><xsl:value-of select="Crop/@width"/>x<xsl:value-of select="Crop/@height"/></span> pixels.
				 <br/>
				 Crop Top-Left: <span class="param"><xsl:value-of select="Crop/@x"/>,<xsl:value-of select="Crop/@y"/> pixels.</span>
				 <br/>
				 Sensor Black: <span class="param"><xsl:value-of select="Sensor/@black"/></span>, White:
				 <span class="param"><xsl:value-of select="Sensor/@white"/>.</span>
				 <br/>
				 Sensor Colors:
				 <xsl:for-each select="CFA/Color">
				 		<span class="param">[<xsl:value-of select="."/>]</span> 
				 </xsl:for-each>.
				 </xsl:if>
      </div>
		 <br/>
 	</xsl:for-each>
  </body>
</html>
</xsl:template>

</xsl:stylesheet>
