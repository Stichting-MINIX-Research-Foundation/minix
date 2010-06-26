<?xml version='1.0' encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0" >
<xsl:output encoding="utf-8" method="html" indent="yes" doctype-public="-//W3C//DTD HTML 4.01 Transitional//EN" />
<xsl:template match="/changelog">
<html>
	<head>
		<title>mdocml - CVS-ChangeLog</title>
		<link rel="stylesheet" href="index.css" type="text/css" media="all" />
	</head>
	<body>
				<xsl:for-each select="entry">
					<div class="clhead">
						<xsl:text>Files modified by </xsl:text>
						<xsl:value-of select="concat(author, ': ', date, ' (', time, ')')" />
					</div>
					<div class="clbody">
						<strong>
							<xsl:text>Note: </xsl:text>
						</strong>
						<xsl:value-of select="msg"/>
						<ul class="clbody">
							<xsl:for-each select="file">
								<li>
									<xsl:value-of select="name"/>
									<span class="rev">
										<xsl:text> &#8212; Rev: </xsl:text>
										<xsl:value-of select="revision"/>
										<xsl:text>, Status: </xsl:text>
										<xsl:value-of select="cvsstate"/>
										<xsl:if test="tag">
											<xsl:text>, Tag: </xsl:text>
											<xsl:value-of select="tag" />
										</xsl:if>
									</span>
								</li>
							</xsl:for-each>
						</ul>
					</div>
				</xsl:for-each>
	</body>
</html>
</xsl:template>
</xsl:stylesheet>
