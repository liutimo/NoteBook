package com.github.liutimo.capability_manage_tool.utils

import com.github.liutimo.capability_manage_tool.entity.CapabilityAttrCategory
import com.github.liutimo.capability_manage_tool.entity.CapabilityAttribute
import org.dom4j.DocumentHelper
import org.dom4j.Element
import org.dom4j.io.OutputFormat
import org.dom4j.io.XMLWriter
import java.io.ByteArrayOutputStream

class CapXMLUtil {
    companion object {
        /**
         * 用于生成能力集文件
         */
        fun convertToXML(categories: List<CapabilityAttrCategory>): String {

            val document = DocumentHelper.createDocument()

            // [1] 创建根节点
            val rootElement = document.addElement("hik_param")

            // [2] 创建子节点
            for (category in categories) {
                // 先添加注释在 添加 节点
                rootElement.addComment(category.categoryRemark)
                val categoryElem = rootElement.addElement(category.categoryName)
                addAttributes(categoryElem, category.attributes)
            }

            // [3] 设置xml格式
            val prettyPrinter = OutputFormat.createPrettyPrint()
            prettyPrinter.encoding = "UTF-8"


            // [4] 生成xml
            val outputStrem = ByteArrayOutputStream()
            val xmlWrite = XMLWriter(outputStrem, prettyPrinter)
            xmlWrite.isEscapeText = false
            xmlWrite.write(document)
            xmlWrite.close()

            return outputStrem.toString();
        }


        /**
         * 添加指定类别的属性
         * @param rootElem      父节点
         * @param attributes    创建attribute 节点，并添加到 rootElem 中
         */
        private fun addAttributes(rootElem: Element, attributes: List<CapabilityAttribute>) {
            for (attribute in attributes) {
                // 先添加注释
                rootElem.addComment(attribute.attrRemark)
                val attrElem = rootElem.addElement(attribute.attrName)
                attrElem.text = attribute.attrValue
            }
        }
    }
}