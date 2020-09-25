package com.github.liutimo.capability_manage_tool.entity

import javax.persistence.*


@Entity
@Table(name = "cap_attribute")
data class CapabilityAttribute constructor(
        @Column(unique = true) val attrName: String,
        @Column val attrRemark: String) {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    @Column(name = "attribute_id")
    private var id = 0

    @ManyToOne(cascade = arrayOf(CascadeType.ALL), fetch = FetchType.EAGER, targetEntity = CapabilityAttrCategory::class)
    @JoinColumn(name = "category_id", referencedColumnName = "categoryID", nullable = false)
    var category :CapabilityAttrCategory? = null


    /**
     *  <attr>attrValue</attr>
     * 该字段有 两个作用于生成 xml
     */
    @Transient
    var attrValue: String = ""

    /**
     * 在能力集文件找那个，存在多种类型的属性。
     * 比如 bool型， 亦或字符串 亦或 数字
     * 其实对应数据库而言，都是 字符串
     * 但是前端 需要根据 其类型来动态设置UI
     *
     * 目前 取值有：
     * list-map
     * bool
     */
    @Column
    var attrValueType: String = ""

    /**
     * 有些属性可能有多个值可以选择。
     * 以芯片平台为例。  有 rk3128 rk3288 rk3399
     * 后续可能还会增加平台
     * 该字段需要记录这些信息。
     * [{1: 'rk3128'}, {2: 'rk3299'}, {3: 'rk3399'}]类似于这种。。
     * 前端解析到这个数据后，就需要生成一个 下拉列表
     *
     * 该字段和 attrValueType紧密关联
     */
    @Column
    var attrValueSet: String = ""


    fun getId(): Int = id
}