package com.github.liutimo.capability_manage_tool.controller

import com.github.liutimo.capability_manage_tool.repository.CapabilityRepository
import com.github.liutimo.capability_manage_tool.entity.CapabilityAttribute
import com.github.liutimo.capability_manage_tool.repository.CapabilityAttrCategoryRepository
import com.github.liutimo.capability_manage_tool.repository.CapabilityAttributeRepository
import com.github.liutimo.capability_manage_tool.utils.LoggerUtil
import org.springframework.beans.factory.annotation.Autowired
import org.springframework.web.bind.annotation.*
import javax.annotation.Resource
import javax.sql.DataSource


@RestController
class CapabilityAttributeController {

    @Resource
    private lateinit var dataSource: DataSource

    @Autowired
    private lateinit var attributeRepository: CapabilityAttributeRepository

    @Autowired
    private lateinit var attrCategoryRepository: CapabilityAttrCategoryRepository

    /**
     * 创建一个新的 属性， 传过来的 是一个 Attribute对象和 所属类别ID
     */
    @PostMapping("/attribute/add")
    fun addAttribute(
            @ModelAttribute attribute: CapabilityAttribute,
            @RequestParam(name = "categoryID") categoryID: Int): Boolean {

        LoggerUtil.logi("${attribute.toString()} + $categoryID)")

        //没有这个类别
        val category = attrCategoryRepository.findById(categoryID)
        if (category.isEmpty) {
            return false
        }

        //保存属性
        attribute.category = category.get()
        attributeRepository.save(attribute)

        //在类别表里面增加一列
        attribute.category!!.categoryName?.let { CapabilityRepository.getInstance(dataSource).addAttribute(it, attribute.attrName) }

        return true
    }


    @DeleteMapping("/attribute/{id}")
    fun delAttribute(@PathVariable(name = "id") id: Int): Boolean {
        val exmaple = attributeRepository.findById(id)
        if (exmaple.isEmpty) {
            LoggerUtil.logi("属性(id = $id) 不存在!!!")
            return false
        }

        val attribute = exmaple.get()

        //删除表中的字段
        attribute.category?.categoryName?.let { CapabilityRepository.getInstance(dataSource).delAttribute(it, attribute.attrName) }

        attributeRepository.delete(attribute)

        return false
    }


    @GetMapping("/attribute/getall")
    fun getAll(): List<CapabilityAttribute> = attributeRepository.findAll()

    @PostMapping("/attribute/update")
    fun updateAttribute(
            @ModelAttribute attribute: CapabilityAttribute,
            @RequestParam(name = "categoryID") categoryID: Int): Boolean {

        // TODO 暂时不实现了，，要修改就先删后加
        return false
    }
}
