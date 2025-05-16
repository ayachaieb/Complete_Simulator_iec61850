import { ComponentFixture, TestBed } from '@angular/core/testing';

import { GooseComponent } from './goose.component';

describe('GooseComponent', () => {
  let component: GooseComponent;
  let fixture: ComponentFixture<GooseComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [GooseComponent]
    })
    .compileComponents();

    fixture = TestBed.createComponent(GooseComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
