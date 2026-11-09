entity "probe" {
    check_free_fields = function(self)
        assert(self.input == nil and self.owner == nil)
        self.owner = 3
        self.input = "free"
        assert(self.owner == 3 and self.input == "free")
    end,
}
