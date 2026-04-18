`timescale 1ns / 1ps

module SPIslave(
    // FPGA input
    input logic RstL,
    input logic clk,
    output logic RXDV,
    output logic [7:0] RXByte,
    input logic TXDV,
    input logic [7:0] TXByte,
    
    // SPI mode 0
    input logic sck,
    input logic nss,
    input logic mosi,
    output logic miso
    );
    
    logic SPIMISOMux;
    
    logic [2:0] rRXBitCount;
    logic [2:0] rTXBitCount;
    logic [7:0] rRXByte_t;
    logic [7:0] rRXByte;
    logic rRXDone, r2RXDone, r3RXDone;
    logic [7:0] rTXByte;
    logic rSPIMISOBit, rPreloadMISO;
    
    always_ff @(posedge sck or posedge nss)
    begin
        if (nss)
        begin
            rRXBitCount <= 0;
            rRXDone <= 1'b0;
        end
        else
        begin
            rRXBitCount <= rRXBitCount + 1;
            
            // receive LSB and shift to MSB
            rRXByte_t <= {rRXByte_t[6:0], mosi};
            
            if (rRXBitCount == 3'b111)
            begin
                rRXDone <= 1'b1;
                rRXByte <= {rRXByte_t[6:0], mosi};
            end
            else if (rRXBitCount == 3'b010)
            begin
                rRXDone <= 1'b0;
            end
        end
    end
    
    always_ff @(posedge clk or negedge RstL)
    begin
        if (~RstL)
        begin
            r2RXDone <= 1'b0;
            r3RXDone <= 1'b0;
            RXDV <= 1'b0;
            RXByte <= 8'h00;
        end
        else
        begin
        // remove metastability
            r2RXDone <= rRXDone;  
            r3RXDone <= rRXDone;    
            
            if (r3RXDone == 1'b0 && r2RXDone == 1'b1) // rising edge
            begin
                RXDV <= 1'b1; // pulse is valid, 1 clock cycle
                RXByte <= rRXByte;
            end
            else
            begin   
                RXDV <= 1'b0;  
            end
        end
    end
    
    always_ff @(posedge sck or posedge nss)
    begin
        if (nss)
        begin
            rPreloadMISO <= 1'b1;
        end
        else
        begin
            rPreloadMISO <= 1'b0;
        end
    end
    
    always_ff @(posedge sck or posedge nss)
    begin
        if (nss)
        begin
            rTXBitCount <= 3'b111;
            rSPIMISOBit <= rTXByte[3'b111];
        end
        else
        begin
            rTXBitCount <= rTXBitCount + 1;
            
            // cross from sys clk to spi clk, could use a FIFO to counteract this
            rSPIMISOBit <= rTXByte[rTXBitCount];
        end
    end
    
    always_ff @(posedge clk or negedge RstL)
    begin
        if (~RstL)
        begin
            rTXByte <= 8'h00;
        end
        else
        begin
            if (TXDV) 
            begin
                rTXByte <= TXByte;
            end
        end
    end
    
    assign SPIMISOMux = rPreloadMISO ? rTXByte[3'b111] : rSPIMISOBit;
    
    assign miso = nss ? 1'bZ : SPIMISOMux;
    
endmodule
