package com.github.liutimo.capability_manage_tool.entity

import org.springframework.context.annotation.ComponentScan
import javax.persistence.*

/**
 * 对应能力集的类别。目前存在的类别是：
 * 1. 能力集文件信息
 * 2. 设备基本信息
 * 3. 设备默认参数
 * 4. 能力与组件支持
 * 5. 视频播放能力集
 */

@Entity
@Table(name = "cap_attribute_category")
data class CapabilityAttrCategory(
        @Column val categoryName: String,
        @Column val categoryRemark: String) {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    @Column
    private var categoryID = 0

    @OneToMany(cascade = arrayOf(CascadeType.ALL), fetch = FetchType.LAZY, targetEntity = CapabilityAttribute::class, mappedBy = "category")
    var attributes: MutableList<CapabilityAttribute> = mutableListOf()

    fun getCategoryID() = categoryID
}